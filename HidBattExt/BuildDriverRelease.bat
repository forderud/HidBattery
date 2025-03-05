:: Script must be run from a "Developer Command Prompt"

echo Building driver:
msbuild.exe  /nologo /verbosity:minimal /property:Configuration="Release";Platform="x64"   ..\HidBattExt.sln || exit /B 1
msbuild.exe  /nologo /verbosity:minimal /property:Configuration="Release";Platform="ARM64" ..\HidBattExt.sln || exit /B 1

echo Packaging driver binaries in CAB file:
makecab.exe /f HidBattExt_x64.ddf || exit /B 1
makecab.exe /f HidBattExt_arm64.ddf || exit /B 1

echo Signing driver CAB:
signtool.exe sign /a /fd sha256 disk1\HidBatttExt_x64.cab || exit /B 1
signtool.exe sign /a /fd sha256 disk1\HidBatttExt_arm64.cab || exit /B 1

echo [done]
