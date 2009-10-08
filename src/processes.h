#pragma once

// Exported routines
extern "C" __declspec(dllexport) void	FindProcess( HWND		hwndParent, 
													 int		string_size,
													 char		*variables, 
													 stack_t	**stacktop );

extern "C" __declspec(dllexport) void	KillProcess( HWND		hwndParent, 
													 int		string_size,
													 char		*variables, 
													 stack_t	**stacktop );

extern "C" __declspec(dllexport) void	FindDevice(  HWND		hwndParent, 
													 int		string_size,
													 char		*variables, 
													 stack_t	**stacktop );

