set Platform=Win64
set Configuration=Debug
set OutDir="./bin/%Platform%/%Configuration%"

cmake -G Ninja -S "./" -B %OutDir% -D CMAKE_C_COMPILER=clang -D CMAKE_CXX_COMPILER=clang++ -D CMAKE_BUILD_TYPE=%Configuration% -D PLATFORM=%Platform%
cd %OutDir%
ninja