mkdir build_win_64tool & pushd build_win_64tool
cmake .. -G "Visual Studio 16 2019" -T v141 -A x64 -DLUAC_COMPATIBLE_FORMAT=ON -DUSE_THREAD=ON
popd
cmake --build build_win_64tool --config Release
pause
