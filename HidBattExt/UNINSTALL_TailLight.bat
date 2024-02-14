@echo off
:: Goto current directory
cd /d "%~dp0"

:: delete the devnode
devcon /r remove "HID\VID_045E&PID_082A&MI_01&Col05"
:: TODO: Switch to PNPUTIL in Win11 21H2
::PNPUTIL /remove-device /deviceid "HID\VID_045E&PID_082A&MI_01&Col05"

:: uninstall driver
PNPUTIL /delete-driver TailLight.inf /uninstall /force /reboot

:: Delete TailLightDeviceInformation WMI class security descriptor
reg delete "HKLM\SYSTEM\CurrentControlSet\Control\WMI\Security" /v "07982702-2086-4B83-9444-34989BB10554" /f

pause
