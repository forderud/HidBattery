@echo off
:: Goto current directory
cd /d "%~dp0"

:: Use DevCon to delete the devnode
devcon /r remove "HID\VID_045E&PID_082A&MI_01&Col05"
:: Use DevCon for uninstallation (fails)
::devcon -f dp_delete TailLight.inf

:: Use PnpUtil for uninstallation (succeeds but devnode leaks)
PNPUTIL /delete-driver TailLight.inf /uninstall /force /reboot
::PNPUTIL /remove-device "HID\VID_045E&PID_082A&MI_01&Col05"

pause
