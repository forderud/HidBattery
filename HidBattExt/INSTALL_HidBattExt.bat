@echo off
:: Goto current directory
cd /d "%~dp0"

:: Trust driver certificate
certutil.exe -addstore root HidBattExt.cer
certutil.exe -f -addstore trustedpublisher HidBattExt.cer

:: Use PnpUtil for installation
PNPUTIL /add-driver HidBattExt.inf /install

pause
