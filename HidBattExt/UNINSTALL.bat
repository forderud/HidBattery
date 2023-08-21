@echo off
:: Goto current directory
cd /d "%~dp0"

PNPUTIL /delete-driver firefly.inf /uninstall /reboot

pause
