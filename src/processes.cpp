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
lpfGetModuleBaseName        GetModuleBaseName;
lpfEnumDeviceDrivers        EnumDeviceDrivers;
lpfGetDeviceDriverBaseName  GetDeviceDriverBaseName;

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

    EnumProcesses           = (lpfEnumProcesses) GetProcAddress(g_hInstLib, "EnumProcesses");
    EnumProcessModules      = (lpfEnumProcessModules) GetProcAddress(g_hInstLib, "EnumProcessModules");
    GetModuleBaseName       = (lpfGetModuleBaseName) GetProcAddress(g_hInstLib, "GetModuleBaseNameA");
    EnumDeviceDrivers       = (lpfEnumDeviceDrivers) GetProcAddress(g_hInstLib, "EnumDeviceDrivers");
    GetDeviceDriverBaseName = (lpfGetDeviceDriverBaseName) GetProcAddress(g_hInstLib, "GetDeviceDriverBaseNameA");

    if (!EnumProcesses || !EnumProcessModules || !EnumDeviceDrivers ||
      !GetModuleBaseName || !GetDeviceDriverBaseName )
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
    GetModuleBaseName = NULL;
    EnumDeviceDrivers = NULL;

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

    if (0 == GetModuleBaseName(hProcess, modulesArray[0], currentProcessName, 1024))
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
				if( GetModuleBaseName( hProcess, phModule[ 0 ], szCurrentProcessName, 1024 ) > 0 )
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


static bool FindDev(char *szDriverName)
{
	char		szDeviceName[ 1024 ];
	char		szCurrentDeviceName[ 1024 ];
	LPVOID		lpDevices[ 1024 ];
	DWORD		dDevicesSize( 1024 );
	DWORD		dSize( 1024 );
	TCHAR		tszCurrentDeviceName[ 1024 ];
	DWORD		dNameSize( 1024 );

	// make the name lower case
	memset( szDeviceName, 0, 1024*sizeof(char) );
	sprintf( szDeviceName, "%s", strlwr( szDriverName ) );

	if( false == LoadPSAPIRoutines() )
		return false;

    if( FALSE == EnumDeviceDrivers( lpDevices, dSize, &dDevicesSize ) )
	{
		FreePSAPIRoutines();

		return false;
	}
	// walk trough and compare see if the device driver exists
	for( int k( dDevicesSize / sizeof( LPVOID ) ); k >= 0; k-- )
	{
		memset( szCurrentDeviceName, 0, 1024*sizeof(char) );
		memset( tszCurrentDeviceName, 0, 1024*sizeof(TCHAR) );

		if( 0 != GetDeviceDriverBaseName( lpDevices[ k ], tszCurrentDeviceName, dNameSize ) )
		{
			sprintf( szCurrentDeviceName, "%S", tszCurrentDeviceName );

			if( 0 == strcmp( strlwr( szCurrentDeviceName ), szDeviceName ) )
			{
				FreePSAPIRoutines();

				return true;
			}
		}
	}

	FreePSAPIRoutines();

	return false;
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

