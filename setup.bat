set configuration=Release
set dir=build
mkdir %dir%
cd %dir%
set platform=x64
cmake .. -DCMAKE_BUILD_TYPE=%configuration% -DCMAKE_GENERATOR_PLATFORM=%platform%
msbuild ALL_BUILD.vcxproj /p:Configuration=%configuration% /p:Platform=%platform%
pause


