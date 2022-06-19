rd /s /q %~dp0objWin32 %~dp0objX64 %~dp0objArm64 %~dp0Release
mkdir %~dp0Release
msbuild %~dp0TextTools.sln -t:Rebuild -p:Configuration=Release;Platform=x86 && zip -j %~dp0Release\TextUtils-X86-32.zip %~dp0objWin32\Release\*.exe
msbuild %~dp0TextTools.sln -t:Rebuild -p:Configuration=Release;Platform=x64 && zip -j %~dp0Release\TextUtils-X86-64.zip %~dp0objX64\Release\*.exe
msbuild %~dp0TextTools.sln -t:Rebuild -p:Configuration=Release;Platform=arm64 && zip -j %~dp0Release\TextUtils-ARM-64.zip %~dp0objArm64\Release\*.exe
