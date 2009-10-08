#include "stdafx.h"
#include "processes.h"
#include "string.h"

// PSAPI function pointers
typedef BOOL	(WINAPI *lpfEnumProcesses)          (DWORD *, DWORD, DWORD *);
typedef BOOL	(WINAPI *lpfEnumProcessModules)     (HANDLE, HMODULE *, DWORD, LPDWORD);
typedef DWORD	(WINAPI *lpfGetModuleBaseName)      (HANDLE, HMODULE, LPTSTR, DWORD);
typedef BOOL	(WINAPI *lpfEnumDeviceDrivers)      (LPVOID *, DWORD, LPDWORD);
typedef BOOL	(WINAPI *lpfGetDeviceDriverBaseName)(LPVOID, LPTSTR, DWORD);

// global variables
lpfEnumProcesses            EnumProcesses;
lpfEnumProcessModules       EnumProcessModules;
lpfGetModuleBaseName        GetModuleBaseNameA;
lpfEnumDeviceDrivers        EnumDeviceDrivers;
lpfGetDeviceDriverBaseName  GetDeviceDriverBaseNameA;

static HINSTANCE       g_hInstance;
static HWND            g_hwndParent;
static HINSTANCE       g_hInstLib;

// main DLL entry
BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG /*ul_reason_for_call*/, LPVOID	/*lpReserved*/)
{
    g_hInstance = (HINSTANCE)hInst;
    return TRUE;
}

static void FreePSAPIRoutines()
{
    EnumProcesses = NULL;
    EnumProcessModules = NULL;
    GetModuleBaseNameA = NULL;
    EnumDeviceDrivers = NULL;
    GetDeviceDriverBaseNameA = NULL;

    if (g_hInstLib) 
    {
        FreeLibrary(g_hInstLib);
        g_hInstLib = NULL;
    }
}

static BOOL HasSAPIRoutines()
{
    return g_hInstLib && EnumProcesses && EnumProcessModules && 
        EnumDeviceDrivers && GetModuleBaseNameA && GetDeviceDriverBaseNameA;
}

// loads the psapi routines
static void LoadPSAPIRoutines()
{
    g_hInstLib = LoadLibraryA("PSAPI.DLL");
    if (!g_hInstLib)
        return;

    EnumProcesses            = (lpfEnumProcesses)GetProcAddress(g_hInstLib, "EnumProcesses");
    EnumProcessModules       = (lpfEnumProcessModules)GetProcAddress(g_hInstLib, "EnumProcessModules");
    GetModuleBaseNameA       = (lpfGetModuleBaseName)GetProcAddress(g_hInstLib, "GetModuleBaseNameA");
    EnumDeviceDrivers        = (lpfEnumDeviceDrivers)GetProcAddress(g_hInstLib, "EnumDeviceDrivers");
    GetDeviceDriverBaseNameA = (lpfGetDeviceDriverBaseName)GetProcAddress(g_hInstLib, "GetDeviceDriverBaseNameA");
}

class AutoSapiRoutines {
public:
    AutoSapiRoutines() { LoadPSAPIRoutines(); }
    ~AutoSapiRoutines() { FreePSAPIRoutines(); }
    BOOL ok() { return HasSAPIRoutines(); }
};

BOOL IsProcessNamed(DWORD processId, char *processName)
{
    HANDLE      hProcess = NULL;
    char        currentProcessName[1024];
    HMODULE     modulesArray[1024];
    DWORD       modulesCount;
    BOOL        nameMatches = FALSE;

    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!hProcess)
        return FALSE;

    BOOL ok = EnumProcessModules(hProcess, modulesArray, sizeof(HMODULE)*1024, &modulesCount);
    if (!ok)
        goto Exit;

    if (0 == GetModuleBaseNameA(hProcess, modulesArray[0], currentProcessName, 1024))
        goto Exit;

    if (0 == _stricmp(currentProcessName, processName))
        nameMatches = TRUE;

Exit:
    CloseHandle(hProcess);
    return nameMatches;
}

#define PIDS_ARRAY_SIZE_MAX 1024
// return true if process with <processName> was found, false otherwise
static BOOL FindProc(char *processName)
{
    DWORD  pidsArray[PIDS_ARRAY_SIZE_MAX];
    DWORD  cbPidsArraySize;

    AutoSapiRoutines sapi;
    if (!sapi.ok())
        return FALSE;

    if (!EnumProcesses(pidsArray, PIDS_ARRAY_SIZE_MAX, &cbPidsArraySize))
        return FALSE;

    int processesCount = cbPidsArraySize / sizeof(DWORD);
    for (int i = 0; i < processesCount; i++)
    {
        if (IsProcessNamed(pidsArray[i], processName)) 
            return TRUE;
    }
    return FALSE;
}

#define TEN_SECONDS_IN_MS 10*1000

// return TRUE if found and killed a process
BOOL KillProcessNamed(DWORD processId, char *processName, BOOL waitUntilTerminated)
{
    HANDLE      hProcess = NULL;
    char        currentProcessName[1024];
    HMODULE     modulesArray[1024];
    DWORD       modulesCount;
    BOOL        killed = FALSE;

    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, processId);
    if (!hProcess)
        return FALSE;

    BOOL ok = EnumProcessModules(hProcess, modulesArray, sizeof(HMODULE)*1024, &modulesCount);
    if (!ok)
        goto Exit;

    if (0 == GetModuleBaseNameA(hProcess, modulesArray[0], currentProcessName, 1024))
        goto Exit;

    if (0 != _stricmp(currentProcessName, processName))
        goto Exit;

    killed = TerminateProcess(hProcess, 0);
    if (!killed)
        goto Exit;

    if (waitUntilTerminated)
        WaitForSingleObject(hProcess, TEN_SECONDS_IN_MS);

    UpdateWindow(FindWindow(NULL, "Shell_TrayWnd"));    
    UpdateWindow(GetDesktopWindow());

Exit:
    CloseHandle(hProcess);
    return killed;
}

static BOOL KillProc(char *processName, BOOL waitUntilTerminated)
{
    DWORD  pidsArray[PIDS_ARRAY_SIZE_MAX];
    DWORD  cbPidsArraySize;
    int    killedCount = 0;

    AutoSapiRoutines sapi;
    if (!sapi.ok())
        return FALSE;

    if (!EnumProcesses(pidsArray, PIDS_ARRAY_SIZE_MAX, &cbPidsArraySize))
        return FALSE;

    int processesCount = cbPidsArraySize / sizeof(DWORD);

    for (int i = 0; i < processesCount; i++)
    {
        if (KillProcessNamed(pidsArray[i], processName, waitUntilTerminated)) 
            killedCount++;
    }

    return killedCount > 0;
}

static BOOL FindDev(char *deviceName)
{
    char        currentDeviceName[1024];
    LPVOID      devicesArray[1024];
    DWORD       cbDevicesArraySize;

    AutoSapiRoutines sapi;
    if (!sapi.ok())
        return FALSE;

    if (!EnumDeviceDrivers(devicesArray, 1024, &cbDevicesArraySize))
        return FALSE;

    int devicesCount = cbDevicesArraySize / sizeof(LPVOID);
    for (int i=0; i < devicesCount; i++)
    {
        if (0 == GetDeviceDriverBaseNameA(devicesArray[i], currentDeviceName, 1024))
            continue;
        
        if (0 == _stricmp(currentDeviceName, deviceName))
            return TRUE;
    }

    return FALSE;
}

static void SetParamFromBool(char *param, BOOL val)
{
    if (val)
        param[0] = '1';
    else
        param[0] = '0';
    param[1] = 0;
}

extern "C" __declspec(dllexport)
void FindProcess(HWND hwndParent, int string_size, char *variables, stack_t **stacktop)
{
    char            szParameter[ 1024 ];
    g_hwndParent    = hwndParent;

    EXDLL_INIT();
    popstring(szParameter);
    BOOL ok = FindProc(szParameter);
    SetParamFromBool(szParameter, ok);
    setuservariable(INST_R0, szParameter);
}

extern "C" __declspec(dllexport)
void KillProcess(HWND hwndParent, int string_size, char *variables, stack_t **stacktop)
{
    char    szParameter[1024];
    g_hwndParent = hwndParent;

    EXDLL_INIT();
    popstring(szParameter);
    BOOL ok = KillProc(szParameter, FALSE);
    SetParamFromBool(szParameter, ok);
    setuservariable(INST_R0, szParameter);
}

extern "C" __declspec(dllexport)
void KillProcessAndWait(HWND hwndParent, int string_size, char *variables, stack_t **stacktop)
{
    char    szParameter[1024];
    g_hwndParent = hwndParent;

    EXDLL_INIT();
    popstring(szParameter);
    BOOL ok = KillProc(szParameter, TRUE);
    SetParamFromBool(szParameter, ok);
    setuservariable(INST_R0, szParameter);
}

extern "C" __declspec(dllexport)
void FindDevice(HWND hwndParent, int string_size, char *variables, stack_t **stacktop)
{
    char    szParameter[1024];
    g_hwndParent = hwndParent;
    EXDLL_INIT();
    popstring(szParameter);
    BOOL ok = FindDev(szParameter);
    SetParamFromBool(szParameter, ok);
    setuservariable(INST_R0, szParameter);
}

