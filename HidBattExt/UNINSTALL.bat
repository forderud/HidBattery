@echo off
:: Goto current directory
cd /d "%~dp0"

PNPUTIL /delete-driver TailLight.inf /uninstall /reboot

pause
