@echo off
:: Goto current directory
cd /d "%~dp0"

:: uninstall driver
PNPUTIL /delete-driver TailLight.inf /uninstall /force

pause
