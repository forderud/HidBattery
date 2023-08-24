@echo off
:: Goto current directory
cd /d "%~dp0"

:: Use DevCon for installation, since it allows providing HWID
devcon /r install TailLight.inf "HID\VID_045E&PID_082A&MI_01&Col05"

:: Use PnpUtil for installation (succeeds but driver isn't loaded)
::devgen /add /bus ROOT /hardwareid "HID\VID_045E&PID_082A&MI_01&Col05"
::PNPUTIL /add-driver TailLight.inf /install /reboot

pause
