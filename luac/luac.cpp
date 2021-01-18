#include <iostream>
#include <fstream>
#include <iostream>
#include <filesystem>

#ifdef USE_THREAD
#include "threadpool.h"
#endif


#ifdef __cplusplus
extern "C" {
#endif
#include "lprefix.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#include "lobject.h"
#include "lstate.h"
#include "lundump.h"
#ifdef __cplusplus
}
#endif

#define PROGNAME "luac"        /* default program name */
#define OUTPUT PROGNAME ".out" /* default output file */

const char *progname = PROGNAME;
const char *output = OUTPUT;

namespace fs = std::filesystem;

static int fatal(lua_State* l, const char *message)
{
    lua_close(l);
    fprintf(stderr, "%s: %s\n", progname, message);
    return EXIT_FAILURE;
}

static int cannot(lua_State* l, const char *what)
{
    lua_close(l);
    fprintf(stderr, "%s: cannot %s %s: %s\n", progname, what, output, strerror(errno));
    return EXIT_FAILURE;
}

#define FUNCTION "(function()end)();"

static const char *reader(lua_State *L, void *ud, size_t *size)
{
    UNUSED(L);
    if ((*(int *)ud)--)
    {
        *size = sizeof(FUNCTION) - 1;
        return FUNCTION;
    }
    else
    {
        *size = 0;
        return NULL;
    }
}

#define toproto(L, i) getproto(L->top + (i))

static int writer(lua_State *L, const void *p, size_t size, void *u)
{
    UNUSED(L);
    return (fwrite(p, size, 1, (FILE *)u) != 1) && (size != 0);
}

static int compile(lua_State *L) 
{
    const char* filename = lua_tostring(L, 1);
    if (luaL_loadfile(L, filename) != LUA_OK){
        char msg[512];
        snprintf(msg, 512, "%s:%d: %s", __FUNCTION__,__LINE__, lua_tostring(L, -1));
        return fatal(L, msg);
    }
        
    const Proto *f = toproto(L, -1);

    FILE *D = fopen(filename, "wb");
    if (D == NULL)
        return cannot(L, "open");
    lua_lock(L);
    luaU_dump(L, f, writer, D, 0);
    lua_unlock(L);
    if (ferror(D))
        return cannot(L, "write");
    if (fclose(D))
        return cannot(L, "close");
    
    return 0;
}

int compile_lua(std::string& filename)
{
    if (filename.empty()) {
        return 1;
    }
    lua_State *L = luaL_newstate();
    if(L == NULL) {
        fatal(L, "cannot create state: not enough memory");
    }

#if 0
    lua_pushcfunction(L, &compile);
    lua_pushstring(L, filename.c_str());
    if (lua_pcall(L, 1, 0, 0) != LUA_OK){
        char msg[512];
        snprintf(msg, "%s:%d: %s", __FUNCTION__,__LINE__, lua_tostring(L, -1));
        return fatal(msg);
    }
#else
    if (luaL_loadfile(L, filename.c_str()) != LUA_OK){
        char msg[512];
        snprintf(msg, 512, "%s:%d: %s", __FUNCTION__,__LINE__, lua_tostring(L, -1));
        return fatal(L, msg);
    }
        
    const Proto *f = toproto(L, -1);

    FILE *D = fopen(filename.c_str(), "wb");
    if (D == NULL)
        return cannot(L, "open");

    luaU_dump(L, f, writer, D, 0);

    if (ferror(D))
        return cannot(L,"write");
    if (fclose(D))
        return cannot(L, "close");
#endif

    lua_close(L);
    return EXIT_SUCCESS;
}

bool ignore(fs::directory_entry entry) {
    auto s=entry.path().string();
    if(s.find(".git") != -1 ||
        s.find(".svn") != -1 ||
        s.find(".DS_Store") != -1 ||
        entry.is_directory()) {
            return true;
        }
    return false;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "please input dir "<<std::endl;
        return EXIT_FAILURE;
    }

    fs::path p(argv[1]);
    fs::directory_entry d(p);
    if(!d.is_directory()) {
        std::cout<<"input is not a directory "<<argv[1] <<std::endl;
        return 1;
    }

#ifdef USE_THREAD
    std::cout << "use thread: " <<std::endl;
    ThreadPool pool(4);
    std::vector< std::future<int> > results;

    for(auto& p: fs::recursive_directory_iterator(d.path().string())) {
        if(ignore(p)) {
            continue;
        }
        
        results.emplace_back(
            pool.enqueue([p] {
                auto filename = p.path().string();
                return compile_lua(filename);
            })
        );
    }

    // for(auto && result: results)
    //     std::cout << result.get() << ' ';
    std::cout << "trans over" << std::endl;
#else
    for(auto& p: fs::recursive_directory_iterator(d.path().string())) {
        if(ignore(p)) {
            continue;
        }
        auto filename = p.path().string();

        int ret = compile_lua(filename);
        if (ret) {
            std::cout<< "FILE: " << p << "luac error."<<std::endl;
        }
    }
#endif 
    return 0;
}