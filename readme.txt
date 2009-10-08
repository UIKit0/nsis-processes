This is a NSIS plugin originally written by Andrei Ciubotaru 
and updated by Krzysztof Kowalczyk. It provides the following
NSIS functions:

Processes::FindProcess <process_name.exe>

  Searches the currently running processes for the given
  process name.

  returns 1 if process was found
          0 if process was not found

Processes::KillProcess <process_name.exe>

  Kills all processes with a given name. Does not wait
  until process is fully gone.

  returns 1 if process was found and killed
          0 if process was not found or could not be killed
            (e.g. due to insuficient rights)

Processes::KillProcessAndWait <process_name.exe>

  Kills all processes with a given name. Will wait untill
  process is killed.

  returns 1 if process was found and killed
          0 if process was not found or could not be killed
            (e.g. due to insuficient rights)

Processes::FindDevice <device_base_name>

  Searches the installed devices drivers for the given
  device base name (IMPORTANT: use base name, not filename)

  returns 1 if device driver was found
          0 if device driver was not found

Usage in NSIS:

    Processes::FindProcess "process_name.exe"
    Pop $R0
    StrCmp $R0 "1" found not_foune
    found:
        ...
    not_found:
        ...

    Processes::KillProcess "process_name.exe"
    Pop $R0
    StrCmp $R0 "1" killed not_killed
    killed:
        ...
    not_killed:
        ...

    Processes::FindDevice "device_base_name"
    Pop $R0
    StrCmp $R0 "1" found not_found
    found:
        ...
    not_found:

Supported platforms are: WinNT,Win2K,WinXP and Win2003 Server.

Releases:
 Version: 1.0.1.1
 Release: 08 October 2009
 Changes: added KillProcAndWait function

 Version: 1.0.1.0
 Release: 24 February 2005

 Version: 1.0.0.1
 Release: 12 December 2004
 Copyright: © 2004 Hardwired. No rights reserved.
            There is no restriction and no guaranty for using
            this software.
  Author:  Andrei Ciubotaru [Hardwired]
           Lead Developer ICode&Ideas SRL (http://www.icode.ro)
           hardwiredteks@gmail.com, hardwired@icode.ro

This code is based on Processes-x64fix.zip latest from http://nsis.sourceforge.net/Processes_plug-in

-------- Original notes from Andrei Ciubotaru ----------------

License:

This software binaries and source-code are free for any kind of use, including commercial use. There is no restriction and no guaranty for using this software and/or its source-code. 
If you use the plug-in and/or its source-code, I would appreciate if my name is mentioned.

Andrei Ciubotaru [Hardwired]
Lead Developer ICode&Ideas SRL (http://www.icode.ro/)
hardwiredteks@gmail.com, hardwired@icode.ro

THANKS

Sunil Kamath for inspiring me. I wanted to use its FindProcDLL
but my requirements made it imposible.

Nullsoft for creating this very powerfull installer. One big,
free and full-featured (hmmm... and guiless for the moment) mean
install machine!:)

ME for being such a great coder...
                                        ... HAHAHAHAHAHAHA!

ONE MORE THING

If you use the plugin or it's source-code, I would apreciate
if my name is mentioned.
