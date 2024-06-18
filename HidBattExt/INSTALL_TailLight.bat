@echo off
:: Goto current directory
cd /d "%~dp0"

:: Trust driver certificate
certutil.exe -addstore root TailLight.cer
certutil.exe -f -addstore trustedpublisher TailLight.cer

:: Use PnpUtil for installation
PNPUTIL /add-driver TailLight.inf /install

pause
