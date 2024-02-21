@echo off
:: Goto current directory
cd /d "%~dp0"

:: Install certificate for silent driver installation and loading (run from administative developer command prompt)
:: certmgr.exe /add TailLight.cer /s /r localMachine root
:: certmgr.exe /add TailLight.cer /s /r localMachine trustedpublisher

:: Use PnpUtil for installation
PNPUTIL /add-driver TailLight.inf /install /reboot

pause
