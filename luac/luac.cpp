#include <iostream>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <tuple>

#ifdef USE_THREAD
#include "threadpool.h"
#endif

#include "cxxopts.h"

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

namespace fs = std::filesystem;


#define PROGNAME "luac"        /* default program name */
#define OUTPUT PROGNAME ".out" /* default output file */

#define CUSTOM_LUAC_VERSION  "0.1.0" // luac version

const char *progname = PROGNAME;
const char *output = OUTPUT;


static std::string input_dir;
static std::string output_dir;
static bool need_resave = false;



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

    std::string_view mod_name = filename;
    mod_name.remove_prefix(input_dir.size());
    if(mod_name[0] == '/') {
        mod_name.remove_prefix(1);
    }
    // std::cout << "new mod name: " << mod_name << std::endl;

    // FILE *readF = fopen(filename.c_str(), "rb");
    if (luaL_loadfilex_custom(L, filename.c_str(), std::string(mod_name).c_str(), NULL) != LUA_OK){
        char msg[512];
        snprintf(msg, 512, "%s:%d: %s", __FUNCTION__,__LINE__, lua_tostring(L, -1));
        return fatal(L, msg);
    }
        
    const Proto *f = toproto(L, -1);

    std::string newfile;
    if(need_resave) {
        std::string s = filename;
        s.replace(0, input_dir.size(),"");
        fs::path p = output_dir;
        newfile = p.string() + s;
        p = newfile.c_str();
        fs::create_directories(p.remove_filename());
    } else {
        newfile = filename;
    }

    // std::cout << "newfile: "<< newfile <<std::endl;


    FILE *D = fopen(newfile.c_str(), "wb");
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
    if(entry.path().extension().string() != ".lua" ||
        entry.is_directory()) {
            return true;
        }
    return false;
}

fs::directory_entry build_dir_entry(std::string dir)
{
    fs::path p(dir);
    fs::directory_entry d(p);
    return d;
}

int main(int argc, char *argv[]) 
{
    cxxopts::Options options("luac", "PixelNeko customed luac aim to compile lua source code to bytecode very quickly");
    options.add_options()
    ("d,dir","Lua code directory", cxxopts::value<std::string>())
    ("o,output","resave the lua code to this directory", cxxopts::value<std::string>()->default_value(""))
    ("v,version", "Customed luac version")
    ("h,help","Print usage");

    auto result = options.parse(argc, argv);

    if(result.count("version")) {
        std::cout << "luac version: "<<CUSTOM_LUAC_VERSION<<std::endl;
        exit(0);
    }

    if(result.count("help") || !result.count("dir")) {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    if(result.count("output")) {
        need_resave = true;
        output_dir = result["output"].as<std::string>();
        if(output_dir.empty()) {
            std::cout<<"error: output dir is empty "<<std::endl<<options.help() <<std::endl;
            exit(1);
        }

        auto d = build_dir_entry(output_dir);
        if(d.is_directory()) {
            std::cout<<"path: "<< output_dir <<" exist. delete it now." <<std::endl;
            fs::remove_all(d);
        }
        bool ok = fs::create_directories(d);
        std::cout << (ok ? "create dir success: ":"create dir failed: ") << d.path() << std::endl;
        if(!ok) {
            exit(1);
        }
    }

    std::string dir = result["dir"].as<std::string>();
    auto d = build_dir_entry(dir);
    if(!d.is_directory()) {
        std::cout<<"input is not a directory "<< dir <<std::endl;
        return 1;
    }
    input_dir = dir;

    bool compile_ok = true;

#ifdef USE_THREAD
    std::cout << "luac in multithread mode " <<std::endl;
    ThreadPool pool(4);
    std::vector< std::future<std::tuple<int, std::string>> > results;

    for(auto& p: fs::recursive_directory_iterator(d.path().string())) {
        if(ignore(p)) {
            continue;
        }
        
        results.emplace_back(
            pool.enqueue([p] {
                auto filename = p.path().string();
                return std::make_tuple(compile_lua(filename), filename);
            })
        );
    }

    for(auto && result: results) {
        auto value = result.get();
        if(std::get<0>(value) != 0) {
            compile_ok = false;
            std::cout << "luac compile failed: " <<  std::get<1>(value) << std::endl;
        }   
    }
#else
    for(auto& p: fs::recursive_directory_iterator(d.path().string())) {
        if(ignore(p)) {
            continue;
        }
        auto filename = p.path().string();

        int ret = compile_lua(filename);
        if (ret) {
            std::cout<< "FILE: " << p.path() << "luac error."<<std::endl;
            compile_ok = false;
        }
    }
#endif 
    return compile_ok?0: 1;
}