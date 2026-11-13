set -e

rm -rf build
mkdir -p build
cd build

#conan install .. --build=missing
conan install .. -s compiler.libcxx=libc++ -s build_type=Debug --build=missing
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j
