// InvProcHandler.cpp

#include "stdafx.h"

#include "bzsddk\string.h"
#include "bzscmn\strbase.h"
#include "bzsddk\commondef.h"
#include "bzscmn\file.h"

#include "AutoHandle.h"

using namespace BazisLib;

// This kernel mode device driver monitors every process creation.  If the executable image has a Zone.Identifier = Internet, the process is terminated and the image name is
// passed to InvService.  InvService will then pass the image name to InvBrowser for protected execution.

// PS callback prototypes
VOID CreateProcessNotifyCallbackEx(__inout PEPROCESS Process, __in HANDLE ProcessId, __in_opt  PPS_CREATE_NOTIFY_INFO CreateInfo);
VOID CreateProcessNotifyCallback(__in HANDLE ParentId, __in HANDLE ProcessId, __in BOOLEAN Create);


// Undocumented Zw thread stuff

typedef NTSTATUS NTAPI ZwQueryInformationProcess_PROC(
	IN HANDLE ProcessHandle,
	IN PROCESSINFOCLASS ProcessInformationClass,
	OUT PVOID ProcessInformation,
	IN ULONG ProcessInformationLength,
	OUT PULONG ReturnLength
	);

ZwQueryInformationProcess_PROC	*ZwQueryInformationProcess;


typedef unsigned char BYTE;

typedef struct _PEB_LDR_DATA {
	BYTE Reserved1[8];
	PVOID Reserved2[3];
	LIST_ENTRY InMemoryOrderModuleList;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _RTL_USER_PROCESS_PARAMETERS {
	BYTE Reserved1[16];
	PVOID Reserved2[10];
	UNICODE_STRING ImagePathName;
	UNICODE_STRING CommandLine;
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

typedef
	VOID
	(NTAPI *PPS_POST_PROCESS_INIT_ROUTINE) (
	VOID
	);

typedef struct _PEB {
	BYTE Reserved1[2];
	BYTE BeingDebugged;
	BYTE Reserved2[1];
	PVOID Reserved3[2];
	PPEB_LDR_DATA Ldr;
	PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
	BYTE Reserved4[104];
	PVOID Reserved5[52];
	PPS_POST_PROCESS_INIT_ROUTINE PostProcessInitRoutine;
	BYTE Reserved6[128];
	PVOID Reserved7[1];
	ULONG SessionId;
} PEB, *PPEB;



// A kernel mode device driver cannot do a CreateProcess().  So, InvService will do that for the driver.
// InvService posts an asynchronous read request via ReadFile().  The driver keeps track of this request in a global IRP.  When the driver wants
// the service to call CreateProcess(), the driver will copy the image name into the IRP, and post it as complete.  The service will wake up and call
// CreateProcess()

PIRP g_Irp;		// global outstanding read request IRP


NTSTATUS CompleteReadRequest(PIRP pIrp, DynamicString &sFullPath)
{
	PIO_STACK_LOCATION pIoStackIrp = IoGetCurrentIrpStackLocation(pIrp);

	DbgPrint("InvProcHandler: CompleteReadRequest starting\n");
	if(pIoStackIrp)
	{
		if (pIrp->MdlAddress)
		{
			PCHAR pWriteDataBuffer;
			pWriteDataBuffer = (PCHAR)MmGetSystemAddressForMdlSafe(pIrp->MdlAddress, NormalPagePriority);

			if(pWriteDataBuffer)
			{                             
				size_t len = pIoStackIrp->Parameters.Write.Length;
				len = min(len, sFullPath.SizeInBytes(true));
				memcpy(pWriteDataBuffer, sFullPath.c_str(), len);
				pIrp->IoStatus.Status = STATUS_SUCCESS;
				pIrp->IoStatus.Information = len;
				IoCompleteRequest(pIrp, IO_NO_INCREMENT);
				DbgPrint("InvProcHandler: CompleteReadRequest finished\n");
			}
		}
	}

	return STATUS_SUCCESS;
}



void InvProcHandlerUnload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS InvProcHandlerCreate(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS InvProcHandlerClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS InvProcHandlerRead(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS InvProcHandlerCleanup(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS InvProcHandlerDefaultHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

#ifdef __cplusplus
extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath);
#endif

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath)
{
	DbgPrint("InvProcHandler: DriverEntry\n");

	UNICODE_STRING DeviceName,Win32Device;
	PDEVICE_OBJECT DeviceObject = NULL;
	NTSTATUS status;
	unsigned i;

	RtlInitUnicodeString(&DeviceName,L"\\Device\\InvProcHandler0");
	RtlInitUnicodeString(&Win32Device,L"\\DosDevices\\InvProcHandler0");

	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
		DriverObject->MajorFunction[i] = InvProcHandlerDefaultHandler;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = InvProcHandlerCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = InvProcHandlerClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = InvProcHandlerRead;
	DriverObject->MajorFunction[IRP_MJ_CLEANUP] = InvProcHandlerCleanup;

	DriverObject->DriverUnload = InvProcHandlerUnload;
	status = IoCreateDevice(DriverObject,
							0,
							&DeviceName,
							FILE_DEVICE_UNKNOWN,
							0,
							FALSE,
							&DeviceObject);
	if (!NT_SUCCESS(status))
		return status;
	if (!DeviceObject)
		return STATUS_UNEXPECTED_IO_ERROR;

	DeviceObject->Flags |= DO_DIRECT_IO;
	DeviceObject->AlignmentRequirement = FILE_WORD_ALIGNMENT;
	status = IoCreateSymbolicLink(&Win32Device, &DeviceName);

	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	ZwQueryInformationProcess = (ZwQueryInformationProcess_PROC*)MmGetSystemRoutineAddress(DDK::StaticString(L"ZwQueryInformationProcess"));

	// Set up PS callback routine(s)

	NTSTATUS	rc;
	// The "Ex" version is Vista+.  Use when XP is gone forever.
	//rc = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyCallbackEx, FALSE);
	//if (rc != STATUS_SUCCESS)
	//DbgPrint("PsSetCreateProcessNotifyRoutineEx failed, NTSTAUS = %x\n", rc);

	rc = PsSetCreateProcessNotifyRoutine(CreateProcessNotifyCallback, FALSE);
	if (rc != STATUS_SUCCESS)
		DbgPrint("InvProcHandler: PsSetCreateProcessNotifyRoutine failed, NTSTAUS = %x\n", rc);

	return STATUS_SUCCESS;
}

void InvProcHandlerUnload(IN PDRIVER_OBJECT DriverObject)
{
	DbgPrint("InvProcHandler: unloading\n");
	if (g_Irp)
	{
		CompleteReadRequest(g_Irp, DynamicString(L""));
		g_Irp = NULL;
	}

	// Unhook our callback routine
	NTSTATUS rc;
	//rc = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyCallbackEx, TRUE);
	//if (rc != STATUS_SUCCESS)
	//DbgPrint("PsSetCreateProcessNotifyRoutineEx unhook failed, NTSTAUS = %x\n", rc);
	rc = PsSetCreateProcessNotifyRoutine(CreateProcessNotifyCallback, TRUE);
	if (rc != STATUS_SUCCESS)
		DbgPrint("PsSetCreateProcessNotifyRoutine unhook failed, NTSTAUS = %x\n", rc);

	UNICODE_STRING Win32Device;
	RtlInitUnicodeString(&Win32Device,L"\\DosDevices\\InvProcHandler0");
	IoDeleteSymbolicLink(&Win32Device);
	IoDeleteDevice(DriverObject->DeviceObject);
	DbgPrint("InvProcHandler: unload complete\n");
}

NTSTATUS InvProcHandlerCreate(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	DbgPrint("InvProcHandler: Create\n");
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS InvProcHandlerClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	DbgPrint("InvProcHandler: Close\n");
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS InvProcHandlerCleanup(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	DbgPrint("InvProcHandler: Cleanup\n");
	if (g_Irp)
	{
		CompleteReadRequest(g_Irp, DynamicString(L""));
		g_Irp = NULL;
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}


NTSTATUS InvProcHandlerDefaultHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	DbgPrint("InvProcHandler: Default\n");
	Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Irp->IoStatus.Status;
}


NTSTATUS InvProcHandlerRead(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	DbgPrint("InvProcHandler: Read\n");
	g_Irp = Irp;				// save the IRP
	IoMarkIrpPending(Irp);
	return STATUS_PENDING;
}


NTSTATUS GetProcessImageName(HANDLE hProcess, String &strImageName)
{
	wchar_t		buf[_MAX_PATH * 2];		// I added some extra space for the image name for all the device stuff at the front of the path
	ULONG	buflen = sizeof(buf);

	NTSTATUS status;
	status = ZwQueryInformationProcess( hProcess, ProcessImageFileName, buf, buflen, &buflen);

	if (status != STATUS_SUCCESS)
		DbgPrint("InvProcHandler: ZwQueryInformationProcess failed, NTSTAUS = %x\n", status);

	strImageName = (PUNICODE_STRING)&buf;
	return status;
}


VOID CreateProcessNotifyCallbackEx(__inout PEPROCESS Process, __in HANDLE hProcess, __in_opt PPS_CREATE_NOTIFY_INFO CreateInfo)
{
	if (CreateInfo != NULL)		// is this a process creation?
	{
		CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
	}
}

VOID CreateProcessNotifyCallback(__in HANDLE ParentId, __in HANDLE hProcess, __in BOOLEAN Create)
{
	if (Create)		// is this a process creation?
	{
		// get a process handle we can actually work with
		CLIENT_ID ClientId = {0};
		ClientId.UniqueProcess = hProcess;
		OBJECT_ATTRIBUTES ObjectAttributes = {0};
		InitializeObjectAttributes(&ObjectAttributes, NULL, OBJ_KERNEL_HANDLE, 0, 0);
		NTSTATUS Status = STATUS_UNSUCCESSFUL;
		auto_handle	ProcessHandle;

		Status = ZwOpenProcess(&ProcessHandle, PROCESS_ALL_ACCESS, &ObjectAttributes, &ClientId);
		if (Status != STATUS_SUCCESS)
		{
			DbgPrint("InvProcHandler: ZwOpenProcess failed, NTSTAUS = %x\n", Status);
			return;
		}

		// build the Zone.Identifier NTFS alternate stream name from the image file name
		DynamicString		sFileName;
		Status = GetProcessImageName(ProcessHandle, sFileName);
		if (Status != STATUS_SUCCESS)
			return;

		DynamicString		sAltStreamFileName;
		sAltStreamFileName = sFileName + L":Zone.Identifier";

		// open the Zone.Identifier alt. stream
		ActionStatus fZIStreamStatus;
		File	fZIStream(sAltStreamFileName.c_str(), FileFlags::ReadAccess, FileFlags::OpenExisting, FileFlags::ShareRead, FileFlags::NormalFile, &fZIStreamStatus);

		if (fZIStreamStatus.GetErrorCode() == Success)
		{
			// check for ZoneId=3 (Internet)
			char buf[80];
			RtlSecureZeroMemory(buf, sizeof(buf));
			fZIStream.Read(buf, sizeof(buf) - 1);
			if (strstr(buf, "ZoneId=3") != NULL)
			{
				// now we have to get the full path name with command line args.
				NTSTATUS Status;
				PEPROCESS pEProcess = NULL;
				// get the EPROCESS structure for the target process
				Status = PsLookupProcessByProcessId(hProcess, &pEProcess);
				if (Status != STATUS_SUCCESS)
				{
					DbgPrint("InvProcHandler: PsLookupProcessByProcessId failed, NTSTAUS = %x\n", Status);
					return;
				}

				// attach the current thread to the address space of the target process so we can see its PEB
				KAPC_STATE kapc_state;
				KeStackAttachProcess(pEProcess, &kapc_state);

				PROCESS_BASIC_INFORMATION pbi;
				ULONG	buflen = sizeof(pbi);
				Status = ZwQueryInformationProcess( ProcessHandle, ProcessBasicInformation, &pbi, sizeof(pbi), &buflen);
				if (Status != STATUS_SUCCESS)
				{
					DbgPrint("InvProcHandler: ZwQueryInformationProcess ProcessBasicInformation failed, NTSTAUS = %x\n", Status);
					return;
				}

				// wrap the full path name in quotes
				sFileName.insert(0, 1, '"');
				sFileName += L"\" ";
#ifdef _AMD64_
				// in x64, the command line is stored at an absolute address inside ProcessParameters
				sFileName += pbi.PebBaseAddress->ProcessParameters->CommandLine.Buffer;	
#else
				// in x32, the command line is stored as an offset from ProcessParameters
				BYTE *pCommandLine = (BYTE*)pbi.PebBaseAddress->ProcessParameters + (unsigned)pbi.PebBaseAddress->ProcessParameters->CommandLine.Buffer;
				sFileName += (WCHAR*)pCommandLine;
#endif
				KeUnstackDetachProcess(&kapc_state);

				// sFileName now contains the full path to the image file (in device format), followed by the exe name (which won't be a full path if typed from a cmd line), followed by all cmd line args
				DbgPrint("InvProcHandler: Command line %S\n", sFileName.c_str());
				if (g_Irp)
				{
					CompleteReadRequest(g_Irp, sFileName);
					g_Irp = NULL;
				}
				else
					DbgPrint("InvProcHandler: No async read request posted\n");

				DbgPrint("InvProcHandler: Terminating PID %d\n", hProcess);

				Status = ZwTerminateProcess(ProcessHandle, STATUS_ACCESS_DENIED);
				if (Status != STATUS_SUCCESS)
				{
					DbgPrint("InvProcHandler: ZwTerminateProcess failed, NTSTAUS = %x\n", Status);
					return;
				}
			}
		}
	}
}


