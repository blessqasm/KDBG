#pragma warning(disable : 4995)

#include "global.h"

// TODO: Refactor types - specifically ptrs and ptr binary operations PBYTE would be nice mixed with ULONG
// TODO: Refactor ptr to stack objects

/*
* Stack frames.
*/

typedef struct _STACK_FRAME_X64
{
  ULONG64 AddrOffset;
  ULONG64 StackOffset;
  ULONG64 FrameOffset;
} STACK_FRAME_X64, * PSTACK_FRAME_X64;

/*
* Kernel utilities.
*/

template<typename FUNCTION>
FUNCTION GetSystemRoutine(PCWCHAR procName)
{
  static FUNCTION functionPointer = NULL;
  if (!functionPointer)
  {
    UNICODE_STRING functionName;
    RtlInitUnicodeString(&functionName, procName);
    functionPointer = (FUNCTION)MmGetSystemRoutineAddress(&functionName);
    if (!functionPointer)
    {
      LOG_ERROR("MmGetSystemRoutineAddress\n");
      return NULL;
    }
  }
  return functionPointer;
}

/*
* Thread utilities.
*/

typedef NTSTATUS(*PSGETCONTEXTTHREAD)(
  PETHREAD Thread,
  PCONTEXT ThreadContext,
  KPROCESSOR_MODE Mode);
typedef NTSTATUS(*PSSETCONTEXTTHREAD)(
  PETHREAD Thread,
  PCONTEXT ThreadContext,
  KPROCESSOR_MODE Mode);

NTSTATUS PsGetContextThread(
  PETHREAD Thread,
  PCONTEXT ThreadContext,
  KPROCESSOR_MODE Mode)
{
  return GetSystemRoutine<PSGETCONTEXTTHREAD>(L"PsGetContextThread")(
    Thread,
    ThreadContext,
    Mode);
}
NTSTATUS PsSetContextThread(
  PETHREAD Thread,
  PCONTEXT ThreadContext,
  KPROCESSOR_MODE Mode)
{
  return GetSystemRoutine<PSSETCONTEXTTHREAD>(L"PsSetContextThread")(
    Thread,
    ThreadContext,
    Mode);
}

VOID DumpContext(PCONTEXT context)
{
  LOG_INFO("Control flags\n");
  LOG_INFO("ContextFlags: %u\n", context->ContextFlags);
  LOG_INFO("MxCsr: %u\n", context->MxCsr);
  LOG_INFO("\n");
  LOG_INFO("Segment registers and processor flags\n");
  LOG_INFO("SegCs: %u\n", context->SegCs);
  LOG_INFO("SegDs: %u\n", context->SegDs);
  LOG_INFO("SegEs: %u\n", context->SegEs);
  LOG_INFO("SegFs: %u\n", context->SegFs);
  LOG_INFO("SegGs: %u\n", context->SegGs);
  LOG_INFO("SegSs: %u\n", context->SegSs);
  LOG_INFO("EFlags: %u\n", context->EFlags);
  LOG_INFO("\n");
  LOG_INFO("Debug registers\n");
  LOG_INFO("Dr0: %llu\n", context->Dr0);
  LOG_INFO("Dr1: %llu\n", context->Dr1);
  LOG_INFO("Dr2: %llu\n", context->Dr2);
  LOG_INFO("Dr3: %llu\n", context->Dr3);
  LOG_INFO("Dr6: %llu\n", context->Dr6);
  LOG_INFO("Dr7: %llu\n", context->Dr7);
  LOG_INFO("\n");
  LOG_INFO("Integer registers\n");
  LOG_INFO("Rax: %llu\n", context->Rax);
  LOG_INFO("Rcx: %llu\n", context->Rcx);
  LOG_INFO("Rdx: %llu\n", context->Rdx);
  LOG_INFO("Rbx: %llu\n", context->Rbx);
  LOG_INFO("Rsp: %llu\n", context->Rsp);
  LOG_INFO("Rbp: %llu\n", context->Rbp);
  LOG_INFO("Rsi: %llu\n", context->Rsi);
  LOG_INFO("Rdi: %llu\n", context->Rdi);
  LOG_INFO("R8: %llu\n", context->R8);
  LOG_INFO("R9: %llu\n", context->R9);
  LOG_INFO("R10: %llu\n", context->R10);
  LOG_INFO("R11: %llu\n", context->R11);
  LOG_INFO("R12: %llu\n", context->R12);
  LOG_INFO("R13: %llu\n", context->R13);
  LOG_INFO("R14: %llu\n", context->R14);
  LOG_INFO("R15: %llu\n", context->R15);
  LOG_INFO("\n");
  LOG_INFO("Program counter\n");
  LOG_INFO("Rip: %llu\n", context->Rip);
  LOG_INFO("\n");
  LOG_INFO("Special debug control registers\n");
  LOG_INFO("DebugControl: %llu\n", context->DebugControl);
  LOG_INFO("LastBranchToRip: %llu\n", context->LastBranchToRip);
  LOG_INFO("LastBranchFromRip: %llu\n", context->LastBranchFromRip);
  LOG_INFO("LastExceptionToRip: %llu\n", context->LastExceptionToRip);
  LOG_INFO("LastExceptionFromRip: %llu\n", context->LastExceptionFromRip);
  LOG_INFO("\n");
}

/*
* PE utilities.
*/

#define PE_ERROR_VALUE (ULONG)-1

typedef PPEB(*PSGETPROCESSPEB)(
  PEPROCESS Process);

PPEB PsGetProcessPeb(
  PEPROCESS process)
{
  return GetSystemRoutine<PSGETPROCESSPEB>(L"PsGetProcessPeb")(
    process);
}

typedef struct _PEB_LDR_DATA
{
  ULONG Length;
  BOOLEAN Initialized;
  PVOID SsHandler;
  LIST_ENTRY InLoadOrderModuleList;
  LIST_ENTRY InMemoryOrderModuleList;
  LIST_ENTRY InInitializationOrderModuleList;
  PVOID EntryInProgress;
} PEB_LDR_DATA, * PPEB_LDR_DATA;
typedef struct _LDR_DATA_TABLE_ENTRY
{
  LIST_ENTRY InLoadOrderLinks;
  LIST_ENTRY InMemoryOrderLinks;
  CHAR Reserved0[0x10];
  PVOID DllBase;
  PVOID EntryPoint;
  ULONG SizeOfImage;
  UNICODE_STRING FullDllName;
  UNICODE_STRING BaseDllName;
} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;
typedef struct _PEB64 {
  CHAR Reserved[0x10];
  PVOID ImageBaseAddress;
  PPEB_LDR_DATA Ldr;
} PEB64, * PPEB64;

ULONG RvaToSection(PIMAGE_NT_HEADERS ntHeaders, ULONG rva)
{
  PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
  USHORT numSections = ntHeaders->FileHeader.NumberOfSections;
  for (USHORT i = 0; i < numSections; i++)
    if (sectionHeader[i].VirtualAddress <= rva)
      if ((sectionHeader[i].VirtualAddress + sectionHeader[i].Misc.VirtualSize) > rva)
        return i;
  return PE_ERROR_VALUE;
}
ULONG RvaToOffset(PIMAGE_NT_HEADERS ntHeaders, ULONG rva, ULONG fileSize)
{
  PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
  USHORT numSections = ntHeaders->FileHeader.NumberOfSections;
  for (USHORT i = 0; i < numSections; i++)
    if (sectionHeader[i].VirtualAddress <= rva)
      if ((sectionHeader[i].VirtualAddress + sectionHeader[i].Misc.VirtualSize) > rva)
      {
        rva -= sectionHeader[i].VirtualAddress;
        rva += sectionHeader[i].PointerToRawData;
        return rva < fileSize ? rva : PE_ERROR_VALUE;
      }
  return PE_ERROR_VALUE;
}
PVOID GetModuleBase(PBYTE ntHeader, ULONG virtualBase)
{
  if ((PBYTE)virtualBase < ntHeader)
  {
    return NULL;
  }
  ULONG rva = (ULONG)((PBYTE)virtualBase - ntHeader);
  PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)ntHeader;
  if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
  {
    LOG_ERROR("Invalid IMAGE_DOS_SIGNATURE\n");
    return NULL;
  }
  PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(ntHeader + dosHeader->e_lfanew);
  if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
  {
    LOG_ERROR("Invalid IMAGE_NT_SIGNATURE\n");
    return NULL;
  }
  PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
  ULONG section = RvaToSection(ntHeaders, rva);
  if (section == PE_ERROR_VALUE)
  {
    LOG_ERROR("Invalid section\n");
    return NULL;
  }
  return (PVOID)(ntHeader + sectionHeader[section].VirtualAddress);
}
ULONG GetModuleExportOffset(PBYTE ntHeader, ULONG fileSize, PCCHAR exportName)
{
  PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)ntHeader;
  if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
  {
    LOG_ERROR("Invalid IMAGE_DOS_SIGNATURE\n");
    return PE_ERROR_VALUE;
  }
  PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(ntHeader + dosHeader->e_lfanew);
  if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
  {
    LOG_ERROR("Invalid IMAGE_NT_SIGNATURE\n");
    return PE_ERROR_VALUE;
  }
  PIMAGE_DATA_DIRECTORY dataDir;
  if (ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
  {
    dataDir = ((PIMAGE_NT_HEADERS64)ntHeaders)->OptionalHeader.DataDirectory;
  }
  else
  {
    dataDir = ((PIMAGE_NT_HEADERS32)ntHeaders)->OptionalHeader.DataDirectory;
  }
  ULONG exportDirRva = dataDir[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
  ULONG exportDirSize = dataDir[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
  ULONG exportDirOffset = RvaToOffset(ntHeaders, exportDirRva, fileSize);
  if (exportDirOffset == PE_ERROR_VALUE)
  {
    LOG_ERROR("Invalid export directory\n");
    return PE_ERROR_VALUE;
  }
  PIMAGE_EXPORT_DIRECTORY exportDir = (PIMAGE_EXPORT_DIRECTORY)(ntHeader + exportDirOffset);
  ULONG numOfNames = exportDir->NumberOfNames;
  ULONG addressOfFunctionsOffset = RvaToOffset(ntHeaders, exportDir->AddressOfFunctions, fileSize);
  ULONG addressOfNameOrdinalsOffset = RvaToOffset(ntHeaders, exportDir->AddressOfNameOrdinals, fileSize);
  ULONG addressOfNamesOffset = RvaToOffset(ntHeaders, exportDir->AddressOfNames, fileSize);
  if (addressOfFunctionsOffset == PE_ERROR_VALUE || addressOfNameOrdinalsOffset == PE_ERROR_VALUE || addressOfNamesOffset == PE_ERROR_VALUE)
  {
    LOG_ERROR("Invalid export directory content\n");
    return PE_ERROR_VALUE;
  }
  PULONG addressOfFunctions = (PULONG)(ntHeader + addressOfFunctionsOffset);
  PUSHORT addressOfNameOrdinals = (PUSHORT)(ntHeader + addressOfNameOrdinalsOffset);
  PULONG addressOfNames = (PULONG)(ntHeader + addressOfNamesOffset);
  ULONG exportOffset = PE_ERROR_VALUE;
  for (ULONG i = 0; i < numOfNames; i++)
  {
    ULONG currentNameOffset = RvaToOffset(ntHeaders, addressOfNames[i], fileSize);
    if (currentNameOffset == PE_ERROR_VALUE)
    {
      continue;
    }
    PCCHAR currentName = (PCCHAR)(ntHeader + currentNameOffset);
    ULONG currentFunctionRva = addressOfFunctions[addressOfNameOrdinals[i]];
    if (currentFunctionRva >= exportDirRva && currentFunctionRva < exportDirRva + exportDirSize)
    {
      continue;
    }
    if (strcmp(currentName, exportName) == 0)
    {
      exportOffset = RvaToOffset(ntHeaders, currentFunctionRva, fileSize);
      break;
    }
  }
  if (exportOffset == PE_ERROR_VALUE)
  {
    LOG_ERROR("Export %s not found\n", exportName);
    return PE_ERROR_VALUE;
  }
  return exportOffset;
}
VOID DumpModuleExports(PBYTE ntHeader, ULONG fileSize)
{
  PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)ntHeader;
  if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
  {
    LOG_ERROR("Invalid IMAGE_DOS_SIGNATURE\n");
  }
  PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(ntHeader + dosHeader->e_lfanew);
  if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
  {
    LOG_ERROR("Invalid IMAGE_NT_SIGNATURE\n");
  }
  PIMAGE_DATA_DIRECTORY dataDir;
  if (ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
  {
    dataDir = ((PIMAGE_NT_HEADERS64)ntHeaders)->OptionalHeader.DataDirectory;
  }
  else
  {
    dataDir = ((PIMAGE_NT_HEADERS32)ntHeaders)->OptionalHeader.DataDirectory;
  }
  ULONG exportDirRva = dataDir[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
  ULONG exportDirSize = dataDir[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
  ULONG exportDirOffset = RvaToOffset(ntHeaders, exportDirRva, fileSize);
  if (exportDirOffset == PE_ERROR_VALUE)
  {
    LOG_ERROR("Invalid export directory\n");
  }
  PIMAGE_EXPORT_DIRECTORY exportDir = (PIMAGE_EXPORT_DIRECTORY)(ntHeader + exportDirOffset);
  ULONG numOfNames = exportDir->NumberOfNames;
  ULONG addressOfFunctionsOffset = RvaToOffset(ntHeaders, exportDir->AddressOfFunctions, fileSize);
  ULONG addressOfNameOrdinalsOffset = RvaToOffset(ntHeaders, exportDir->AddressOfNameOrdinals, fileSize);
  ULONG addressOfNamesOffset = RvaToOffset(ntHeaders, exportDir->AddressOfNames, fileSize);
  if (addressOfFunctionsOffset == PE_ERROR_VALUE || addressOfNameOrdinalsOffset == PE_ERROR_VALUE || addressOfNamesOffset == PE_ERROR_VALUE)
  {
    LOG_ERROR("Invalid export directory content\n");
  }
  PULONG addressOfFunctions = (PULONG)(ntHeader + addressOfFunctionsOffset);
  PUSHORT addressOfNameOrdinals = (PUSHORT)(ntHeader + addressOfNameOrdinalsOffset);
  PULONG addressOfNames = (PULONG)(ntHeader + addressOfNamesOffset);
  ULONG exportOffset = PE_ERROR_VALUE;
  LOG_INFO("Exports:\n");
  for (ULONG i = 0; i < numOfNames; i++)
  {
    ULONG currentNameOffset = RvaToOffset(ntHeaders, addressOfNames[i], fileSize);
    if (currentNameOffset == PE_ERROR_VALUE)
    {
      continue;
    }
    PCCHAR currentName = (PCCHAR)(ntHeader + currentNameOffset);
    ULONG currentFunctionRva = addressOfFunctions[addressOfNameOrdinals[i]];
    if (currentFunctionRva >= exportDirRva && currentFunctionRva < exportDirRva + exportDirSize)
    {
      continue;
    }
    exportOffset = RvaToOffset(ntHeaders, currentFunctionRva, fileSize);
    LOG_INFO("\t%X %s\n", exportOffset, currentName);
  }
}

/*
* Memory utilities.
*/

VOID ReadVirtualMemory()
{

}
VOID WriteVirtualMemory()
{

}

/*
* Scanning utilities.
*/

PINT SignedIntScan(PBYTE base, SIZE_T size, INT value)
{
  PINT result = NULL;
  SIZE_T offset = 0;
  while ((base + offset) < (base + size))
  {
    result = (PINT)(base + offset);
    offset += sizeof(INT);
    LOG_INFO("Searching %p -> %d", result, *result);
    if (*result == value)
    {
      LOG_INFO("Found %p -> %d", result, *result);
      break;
    }
  }
  return result;
}
VOID ContextScan(HANDLE tid, SIZE_T iterations)
{
  NTSTATUS status = STATUS_SUCCESS;
  PETHREAD thread = NULL;
  PCONTEXT context = NULL;
  SIZE_T contextSize = sizeof(CONTEXT);
  status = PsLookupThreadByThreadId(tid, &thread);
  LOG_ERROR_IF_NOT_SUCCESS(status, "PsLookupThreadByThreadId %X\n", status);
  status = ZwAllocateVirtualMemory(ZwCurrentProcess(), (PVOID*)&context, 0, &contextSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  LOG_ERROR_IF_NOT_SUCCESS(status, "ZwAllocateVirtualMemory %X\n", status);
  RtlZeroMemory(context, contextSize);
  LOG_INFO("Rax Rcx Rdx Rbx Rsp Rbp Rsi Rdi\n");
  for (SIZE_T i = 0; i < iterations; ++i)
  {
    context->ContextFlags = CONTEXT_ALL;
    status = PsGetContextThread(thread, context, UserMode);
    LOG_ERROR_IF_NOT_SUCCESS(status, "PsGetContextThread %X\n", status);
    LOG_INFO("%llu %llu %llu %llu %llu %llu %llu %llu\n", context->Rax, context->Rcx, context->Rdx, context->Rbx, context->Rsp, context->Rbp, context->Rsi, context->Rdi);
  }
  if (context)
  {
    ZwFreeVirtualMemory(ZwCurrentProcess(), (PVOID*)context, &contextSize, MEM_RELEASE);
  }
  ObDereferenceObject(thread);
}
VOID StackScan(HANDLE pid, HANDLE tid, PWCHAR moduleName, SIZE_T iterations)
{
  NTSTATUS status = STATUS_SUCCESS;
  PEPROCESS process = NULL;
  PETHREAD thread = NULL;
  PCONTEXT context = NULL;
  SIZE_T contextSize = sizeof(CONTEXT);
  PLDR_DATA_TABLE_ENTRY modules = NULL;
  PLDR_DATA_TABLE_ENTRY module = NULL;
  PLIST_ENTRY moduleList = NULL;
  PLIST_ENTRY moduleEntry = NULL;
  STACK_FRAME_X64 stackFrame64;
  PPEB64 peb = NULL;
  KAPC_STATE apc;
  // Get context infos
  status = PsLookupProcessByProcessId(pid, &process);
  LOG_ERROR_IF_NOT_SUCCESS(status, "PsLookupProcessByProcessId %X", status);
  status = PsLookupThreadByThreadId(tid, &thread);
  LOG_ERROR_IF_NOT_SUCCESS(status, "PsLookupThreadByThreadId %X\n", status);
  status = ZwAllocateVirtualMemory(ZwCurrentProcess(), (PVOID*)&context, 0, &contextSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  LOG_ERROR_IF_NOT_SUCCESS(status, "ZwAllocateVirtualMemory %X\n", status);
  // Attach to process
  KeStackAttachProcess(process, &apc);
  // Get module base
  peb = (PPEB64)PsGetProcessPeb(process);
  LOG_ERROR_IF_NOT(!peb, "PsGetProcessPeb\n");
  modules = CONTAINING_RECORD(peb->Ldr->InLoadOrderModuleList.Flink, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
  moduleList = modules->InLoadOrderLinks.Flink;
  moduleEntry = moduleList->Flink;
  while (moduleEntry != moduleList)
  {
    module = CONTAINING_RECORD(moduleEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
    if (wcscmp(moduleName, module->BaseDllName.Buffer) == 0)
    {
      break;
    }
    moduleEntry = moduleEntry->Flink;
  }
  LOG_ERROR_IF_NOT(!module, "Module not found\n");
  PVOID moduleBase = GetModuleBase((PBYTE)module->DllBase, *(PULONG)module->EntryPoint);
  LOG_INFO("ModuleBase %p\n", moduleBase);
  //ULONG moduleExportOffset = GetModuleExportOffset((PBYTE)module->DllBase, module->SizeOfImage, "");
  //LOG_INFO("moduleExportOffset %p\n", moduleExportOffset);
  // Dump exports
  DumpModuleExports((PBYTE)module->DllBase, module->SizeOfImage);
  // Dump stack frames
  LOG_INFO("\n");
  LOG_INFO("Stack Frames:\n");
  for (SIZE_T i = 0; i < iterations; ++i)
  {
    // Get context
    status = PsGetContextThread(thread, context, UserMode);
    LOG_ERROR_IF_NOT_SUCCESS(status, "PsGetContextThread %X\n", status);
    // Get stack frame
    stackFrame64.AddrOffset = context->Rip; // Instruction ptr
    stackFrame64.StackOffset = context->Rsp; // Stack ptr
    stackFrame64.FrameOffset = context->Rbp; // Stack base ptr
    LOG_INFO("%4X %llu %llu %llu\n", i, stackFrame64.AddrOffset, stackFrame64.StackOffset, stackFrame64.FrameOffset);
  }
  // Detach from process
  KeUnstackDetachProcess(&apc);
  // Cleanup
  if (context)
  {
    ZwFreeVirtualMemory(ZwCurrentProcess(), (PVOID*)context, &contextSize, MEM_RELEASE);
  }
  ObDereferenceObject(thread);
  ObDereferenceObject(process);
}

/*
* Communication device.
*/

#define KMOD_DEVICE_NAME L"\\Device\\KMOD"
#define KMOD_DEVICE_SYMBOL_NAME L"\\DosDevices\\KMOD"

#define KMOD_EXEC CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0100, METHOD_OUT_DIRECT, FILE_SPECIAL_ACCESS)

PDEVICE_OBJECT Device = NULL;

VOID CreateDevice(PDRIVER_OBJECT driver, PDEVICE_OBJECT* device, PCWCHAR deviceName, PCWCHAR symbolicName)
{
  NTSTATUS status = STATUS_SUCCESS;
  UNICODE_STRING deviceNameTmp;
  UNICODE_STRING symbolicNameTmp;
  // Create I/O device
  RtlInitUnicodeString(&deviceNameTmp, deviceName);
  RtlInitUnicodeString(&symbolicNameTmp, symbolicName);
  status = IoCreateDevice(driver, 0, &deviceNameTmp, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, 0, device);
  LOG_ERROR_IF_NOT_SUCCESS(status, "IoCreateDevice %X\n", status);
  (*device)->Flags |= (DO_DIRECT_IO | DO_BUFFERED_IO);
  (*device)->Flags &= ~DO_DEVICE_INITIALIZING;
  // Create symbolic link
  status = IoCreateSymbolicLink(&symbolicNameTmp, &deviceNameTmp);
  LOG_ERROR_IF_NOT_SUCCESS(status, "IoCreateSymbolicLink %X\n", status);
}
VOID DeleteDevice(PDEVICE_OBJECT device, PCWCHAR symbolicName)
{
  NTSTATUS status = STATUS_SUCCESS;
  UNICODE_STRING symbolicNameTmp;
  // Delete symbolic link
  RtlInitUnicodeString(&symbolicNameTmp, symbolicName);
  status = IoDeleteSymbolicLink(&symbolicNameTmp);
  LOG_ERROR_IF_NOT_SUCCESS(status, "IoDeleteSymbolicLink %X\n", status);
  // Delete I/O device
  IoDeleteDevice(device);
}

NTSTATUS OnIrpDflt(PDEVICE_OBJECT device, PIRP irp)
{
  UNREFERENCED_PARAMETER(device);
  irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
  irp->IoStatus.Information = 0;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  return irp->IoStatus.Status;
}
NTSTATUS OnIrpCreate(PDEVICE_OBJECT device, PIRP irp)
{
  UNREFERENCED_PARAMETER(device);
  LOG_INFO("Received create request\n");
  irp->IoStatus.Status = STATUS_SUCCESS;
  irp->IoStatus.Information = 0;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  return irp->IoStatus.Status;
}
NTSTATUS OnIrpCtrl(PDEVICE_OBJECT device, PIRP irp)
{
  UNREFERENCED_PARAMETER(device);
  LOG_INFO("Received ctrl request\n");
  irp->IoStatus.Status = STATUS_SUCCESS;
  irp->IoStatus.Information = 0;
  PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
  switch (stack->Parameters.DeviceIoControl.IoControlCode)
  {
    case KMOD_EXEC:
    {
      __try
      {
        ULONG tid = *(PULONG)MmGetSystemAddressForMdl(irp->MdlAddress);
        LOG_INFO("Received tid %u\n", tid);
        ContextScan((HANDLE)tid, 100);
        irp->IoStatus.Status = STATUS_SUCCESS;
        irp->IoStatus.Information = 0;
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
        LOG_ERROR("Something went wrong\n");
        irp->IoStatus.Status = STATUS_UNHANDLED_EXCEPTION;
        irp->IoStatus.Information = 0;
      }
      break;
    }
  }
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  return irp->IoStatus.Status;
}
NTSTATUS OnIrpClose(PDEVICE_OBJECT device, PIRP irp)
{
  UNREFERENCED_PARAMETER(device);
  LOG_INFO("Received close request\n");
  irp->IoStatus.Status = STATUS_SUCCESS;
  irp->IoStatus.Information = 0;
  IoCompleteRequest(irp, IO_NO_INCREMENT);
  return irp->IoStatus.Status;
}

/*
* Entry point.
*/

VOID DriverUnload(PDRIVER_OBJECT driver)
{
  UNREFERENCED_PARAMETER(driver);
  NTSTATUS status = STATUS_SUCCESS;
  // Delete devices
  DeleteDevice(Device, KMOD_DEVICE_SYMBOL_NAME);
  LOG_INFO_IF_SUCCESS(status, "DeInitialized\n");
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING regPath)
{
  UNREFERENCED_PARAMETER(regPath);
  NTSTATUS status = STATUS_SUCCESS;
  // Register driver callbacks
  driver->DriverUnload = DriverUnload;
  // Register default interrupt callbacks
  for (ULONG i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; ++i)
    driver->MajorFunction[i] = OnIrpDflt;
  // Register interrupt callbacks
  driver->MajorFunction[IRP_MJ_CREATE] = OnIrpCreate;
  driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = OnIrpCtrl;
  driver->MajorFunction[IRP_MJ_CLOSE] = OnIrpClose;
  // Create devices
  CreateDevice(driver, &Device, KMOD_DEVICE_NAME, KMOD_DEVICE_SYMBOL_NAME);
  LOG_INFO_IF_SUCCESS(status, "Initialized\n");
  return status;
}