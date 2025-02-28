:: Script must be run from a "Developer Command Prompt"

echo Building driver:
msbuild.exe  /nologo /verbosity:minimal /property:Configuration="Release";Platform="x64"   ..\HidBattExt.sln || exit /B 1
::msbuild.exe  /nologo /verbosity:minimal /property:Configuration="Release";Platform="ARM64" ..\HidBattExt.sln || exit /B 1

echo Packaging driver binaries in CAB file:
makecab.exe /f HidBattExt.ddf || exit /B 1

echo Signing driver CAB:
signtool.exe sign /a /fd sha256 disk1\HidBatttExt.cab || exit /B 1

echo [done]
