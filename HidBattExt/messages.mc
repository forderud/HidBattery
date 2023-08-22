;#ifndef __MESSAGES_H__
;#define __MESSAGES_H__

MessageIdTypedef=NTSTATUS

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )

FacilityNames=(System=0x0
               RpcRuntime=0x2:FACILITY_RPC_RUNTIME
               RpcStubs=0x3:FACILITY_RPC_STUBS
               Io=0x4:FACILITY_IO_ERROR_CODE
              )


MessageId=0x0001 Facility=Io Severity=Error SymbolicName=TailLight_SAFETY
Language=English
Color %2 exceeded safety threshold. Resetting color to RED to indicate failure.
.
;#endif // __MESSAGES_H__
