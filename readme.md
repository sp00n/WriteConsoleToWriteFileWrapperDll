Visual Studio 2019 C++ project to create the 32 bit and 64 bit DLL files to hook into WriteConsoleW calls, and redirect them to WriteFile calls instead. It will also create a file with the output if a /dlllog: argument is provided to the command line argument.

More precisely, it is used to generate a log file for [y-cruncher](http://www.numberworld.org/y-cruncher/), which it normally doesn't.


Can be used with [WriteConsoleToWriteFileWrapperExe](https://github.com/sp00n/WriteConsoleToWriteFileWrapperExe) or the original [withdll.exe from Detours](https://github.com/microsoft/Detours/tree/4.0.1/samples/withdll).
[Detours](https://github.com/microsoft/Detours) 4.0.1 is required for this and is included in the /packages folder.