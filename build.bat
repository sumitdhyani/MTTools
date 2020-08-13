mkdir build
cd build
cmake ..
msbuild MTTools.sln /p:Configuration=Release
msbuild MTTools.sln /p:Configuration=Debug
pause

