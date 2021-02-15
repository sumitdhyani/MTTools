set configuration=Debug
set dir=build_win_%configuration%
mkdir %dir%
cd %dir%
set platform=x64
cmake .. -DCMAKE_BUILD_TYPE=%configuration% -DCMAKE_GENERATOR_PLATFORM=%platform%
msbuild MTTools.sln /property:Configuration=%configuration% /property:Platform=%platform%
pause


