# Windows HidBatt extension filter driver
`HidBattExt` driver that extends the in-built `HidBatt` driver in Windows to also parse and report `CycleCount` and `Temperature` battery parameters.

The HidBatt drivers inability to report these parameters have already been reported to Microsoft in https://aka.ms/AAu4w9g and https://aka.ms/AAu4w8p . The driver will no longer be needed if or when Microsoft improves their `HidBatt` driver.

## Description
The filter driver places itself both _above_ and _below_ HidBatt:  
![image](https://github.com/user-attachments/assets/33d408cf-95e1-4501-9363-cfe81e34bf4c)

The driver instance _below_ HidBatt first filters the HID Power Device (PD) communication with the battery to pick up the missing `CycleCount` and `Temperature` parameters. The driver instance _above_ HidBatt afterwards filters [`IOCTL_BATTERY_QUERY_INFORMATION`](https://learn.microsoft.com/en-us/windows/win32/power/ioctl-battery-query-information) communication to include these parameters in responses.

### Open issues
**WARNING**: The driver is still **work in progress** and only partially working. See  [_windows_ issues](https://github.com/forderud/HidBattery/issues?q=is%3Aissue%20state%3Aopen%20label%3Awindows) for a list of known issues and improvement plans.

### Driver testing
See [Driver testing](https://github.com/forderud/IntelliMouseDriver/wiki/Driver-testing) for an introduction to how to install and test the driver on a dedicated Windows computer with `testsigning` enabled.

## Background
The driver is based on the [TailLight](https://github.com/forderud/IntelliMouseDriver/tree/main/TailLight) driver for Microsoft Pro IntelliMouse, which again is based on the Microsoft [firefly](https://github.com/microsoft/Windows-driver-samples/tree/main/hid/firefly) sample driver.

The filter driver design is furthermore influenced by valuable feedback from _Tim Roberts_, _Mark Roddy_ and _Doron Holan_ in the OSR [How to continuously obtain HID input reports from non-HIDClass driver](https://community.osr.com/t/how-to-continuously-obtain-hid-input-reports-from-non-hidclass-driver/59447/19) forum posting.

## Documentation updates
Windows driver documentation updates triggered by this driver:
* [Extend WdfRequestSetCompletionRoutine to mention need for request formatting](https://github.com/MicrosoftDocs/windows-driver-docs-ddi/pull/1601)
