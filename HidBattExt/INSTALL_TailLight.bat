@echo off
:: Goto current directory
cd /d "%~dp0"

:: Install driver certificate
certmgr.exe /add TailLight.cer /s /r localMachine root
certmgr.exe /add TailLight.cer /s /r localMachine trustedpublisher

:: Use PnpUtil for installation
PNPUTIL /add-driver TailLight.inf /install /reboot

pause
