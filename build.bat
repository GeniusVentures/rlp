cd build
cd Windows
if not exist "Release" mkdir Release
cd Release
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel 16 --config Release