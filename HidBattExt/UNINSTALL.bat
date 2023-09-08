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

:: Delete TailLightDeviceInformation WMI class security descriptor
reg delete "HKLM\SYSTEM\CurrentControlSet\Control\WMI\Security" /v "07982702-2086-4B83-9444-34989BB10554" /f

pause
