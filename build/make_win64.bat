mkdir build_win_64tool & pushd build_win_64tool
cmake ../src -G "Visual Studio 16 2019" -T v141 -A x64 -DNEKOFS_MAKE_TOOLS_LIB=ON
popd
cmake --build build_win_64tool --config Release
pause