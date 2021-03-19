mkdir build32 & pushd build32
cmake .. -G "Visual Studio 16 2019" -T v141 -A x86 -DLUAC_COMPATIBLE_FORMAT=ON -DUSE_THREAD=ON
popd
cmake --build build32 --config Release
pause