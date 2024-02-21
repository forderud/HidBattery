@echo off
:: Goto current directory
cd /d "%~dp0"

:: uninstall driver
PNPUTIL /delete-driver TailLight.inf /uninstall /force /reboot

:: Delete TailLightDeviceInformation WMI class security descriptor
reg delete "HKLM\SYSTEM\CurrentControlSet\Control\WMI\Security" /v "07982702-2086-4B83-9444-34989BB10554" /f

pause
