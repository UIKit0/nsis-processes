#include "stdafx.h"
#include "processes.h"
#include "string.h"

// PSAPI function pointers
typedef BOOL	(WINAPI *lpfEnumProcesses)			( DWORD *, DWORD, DWORD * );
typedef BOOL	(WINAPI *lpfEnumProcessModules)		( HANDLE, HMODULE *, DWORD, LPDWORD );
typedef DWORD	(WINAPI *lpfGetModuleBaseName)		( HANDLE, HMODULE, LPTSTR, DWORD );
typedef BOOL	(WINAPI *lpfEnumDeviceDrivers)		( LPVOID *, DWORD, LPDWORD );
typedef BOOL	(WINAPI *lpfGetDeviceDriverBaseName)( LPVOID, LPTSTR, DWORD );

// global variables
lpfEnumProcesses            EnumProcesses;
lpfEnumProcessModules       EnumProcessModules;
lpfGetModuleBaseName        GetModuleBaseNameA;
lpfEnumDeviceDrivers        EnumDeviceDrivers;
lpfGetDeviceDriverBaseName  GetDeviceDriverBaseNameA;

HINSTANCE       g_hInstance;
HWND            g_hwndParent;
HINSTANCE       g_hInstLib;

// main DLL entry
BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG /*ul_reason_for_call*/, LPVOID	/*lpReserved*/ )
{
    g_hInstance = (struct HINSTANCE__ *)hInst;
    return TRUE;
}

// loads the psapi routines
static bool LoadPSAPIRoutines(void)
{
    g_hInstLib = LoadLibraryA("PSAPI.DLL");
    if (NULL == g_hInstLib)
        return false;

    EnumProcesses            = (lpfEnumProcesses) GetProcAddress(g_hInstLib, "EnumProcesses");
    EnumProcessModules       = (lpfEnumProcessModules) GetProcAddress(g_hInstLib, "EnumProcessModules");
    GetModuleBaseNameA       = (lpfGetModuleBaseName) GetProcAddress(g_hInstLib, "GetModuleBaseNameA");
    EnumDeviceDrivers        = (lpfEnumDeviceDrivers) GetProcAddress(g_hInstLib, "EnumDeviceDrivers");
    GetDeviceDriverBaseNameA = (lpfGetDeviceDriverBaseName) GetProcAddress(g_hInstLib, "GetDeviceDriverBaseNameA");

    if (!EnumProcesses || !EnumProcessModules || !EnumDeviceDrivers ||
      !GetModuleBaseNameA || !GetDeviceDriverBaseNameA )
    {
        FreeLibrary(g_hInstLib);
        return false;
    }

    return true;
}

static void FreePSAPIRoutines(void)
{
    EnumProcesses = NULL;
    EnumProcessModules = NULL;
    GetModuleBaseNameA = NULL;
    EnumDeviceDrivers = NULL;
    GetDeviceDriverBaseNameA = NULL;

    FreeLibrary(g_hInstLib);
}

BOOL IsProcessNamed(DWORD processId, char *processName)
{
    HANDLE      hProcess = NULL;
    char        currentProcessName[1024];
    HMODULE     modulesArray[1024];
    DWORD       modulesCount;
    BOOL        nameMatches = FALSE;

    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, processId);
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

    if (!LoadPSAPIRoutines())
        return FALSE;

    if (!EnumProcesses(pidsArray, PIDS_ARRAY_SIZE_MAX, &cbPidsArraySize))
        goto Error;

    int processesCount = cbPidsArraySize / sizeof(DWORD);

    for (int i = 0; i < processesCount; i++)
    {
        if (IsProcessNamed(pidsArray[i], processName)) 
        {
            FreePSAPIRoutines();
            return TRUE;
        }
    }

Error:
    FreePSAPIRoutines();
    return FALSE;
}

// Kills a process by name. Returns true if process was found, false otherwise.
static bool KillProc(char *szProcess)
{
	char		szProcessName[ 1024 ];
	char		szCurrentProcessName[ 1024 ];
	DWORD		dPID[ 1024 ];
	DWORD		dPIDSize( 1024 );
	DWORD		dSize( 1024 );
	HANDLE		hProcess;
	HMODULE		phModule[ 1024 ];

	// make the name lower case
	memset( szProcessName, 0, 1024*sizeof(char) );
	sprintf( szProcessName, "%s", szProcess );
	strlwr( szProcessName );

	if( false == LoadPSAPIRoutines() )
		return false;

    if( FALSE == EnumProcesses( dPID, dSize, &dPIDSize ) )
	{
		FreePSAPIRoutines();

		return false;
	}

	// walk trough and compare see if the process is running
	for( int k( dPIDSize / sizeof( DWORD ) ); k >= 0; k-- )
	{
		memset( szCurrentProcessName, 0, 1024*sizeof(char) );

		if( NULL != ( hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, dPID[ k ] ) ) )
		{
			if( TRUE == EnumProcessModules( hProcess, phModule, sizeof(HMODULE)*1024, &dPIDSize ) )
				if( GetModuleBaseNameA( hProcess, phModule[ 0 ], szCurrentProcessName, 1024 ) > 0 )
				{
					strlwr( szCurrentProcessName );

					if( NULL != strstr( szCurrentProcessName, szProcessName ) )
					{
						FreePSAPIRoutines();

						if( false == TerminateProcess( hProcess, 0 ) )
						{
							CloseHandle( hProcess );

							return true;
						}

						UpdateWindow( FindWindow( NULL, "Shell_TrayWnd" ) );

						UpdateWindow( GetDesktopWindow() );
						CloseHandle( hProcess );

						return true;
					}
				}

			CloseHandle( hProcess );
		}
	}
	
	FreePSAPIRoutines();

	return false;
}

static BOOL FindDev(char *deviceName)
{
    char        currentDeviceName[1024];
    LPVOID      devicesArray[1024];
    DWORD       cbDevicesArraySize;

    if (!LoadPSAPIRoutines() )
        return FALSE;

    if (!EnumDeviceDrivers(devicesArray, 1024, &cbDevicesArraySize))
        goto Exit;

    int devicesCount = cbDevicesArraySize / sizeof(LPVOID);
    for (int i=0; i < devicesCount; i++)
    {
        if (0 == GetDeviceDriverBaseNameA(devicesArray[i], currentDeviceName, 1024))
            continue;
        
        if (0 == _stricmp(currentDeviceName, deviceName))
        {
            FreePSAPIRoutines();
            return TRUE;
        }
    }

Exit:
    FreePSAPIRoutines();
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
    popstring( szParameter );
    BOOL ok = KillProc(szParameter);
    SetParamFromBool(szParameter, ok);
    setuservariable( INST_R0, szParameter );
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

