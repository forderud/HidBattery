@echo off
:: Goto current directory
cd /d "%~dp0"

:: uninstall driver
PNPUTIL /delete-driver HidBattExt.inf /uninstall /force

pause
