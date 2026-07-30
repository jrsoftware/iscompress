@echo off

rem  Inno Setup
rem  Copyright (C) 1997-2026 Jordan Russell
rem  Portions by Martijn Laan
rem  For conditions of distribution and use, see LICENSE.TXT.
rem
rem  Batch file to prepare isb(un)zip.dll, is(un)zlib.dll, is(un)zstd.dll, the x64 versions, and iszstd-Arm64EC.dll
rem
rem  This batch files does the following things:
rem  -Compile x86 isb(un)zip.dll, is(un)zlib.dll, and is(un)zstd.dll
rem  -Compile x64 isb(un)zip-x64.dll, is(un)zlib-x64.dll, and is(un)zstd-x64.dll
rem  -Compile ARM64EC iszstd-Arm64EC.dll
rem  -Copy them to issrc Files
rem  -Synch them to issrc Projects\Bin (optional)

setlocal

cd /d %~dp0

if exist buildsettings.bat goto buildsettingsfound
:buildsettingserror
echo buildsettings.bat is missing or incomplete. It needs to be created
echo with the following line, adjusted for your system:
echo.
echo   set ISSRCROOT=C:\Issrc
goto failed2

:buildsettingsfound
set ISSRCROOT=
call .\buildsettings.bat
if "%ISSRCROOT%"=="" goto buildsettingserror

call .\compile.bat x86 %1
if errorlevel 1 goto failed
echo Compiling x86 done

call .\compile.bat x64 %1
if errorlevel 1 goto failed
echo Compiling x64 done

call .\compile.bat arm64ec %1
if errorlevel 1 goto failed
echo Compiling arm64ec done

echo - Copying files to issrc\Files
copy bzlib\Win32\Release\isbunzip.dll "%ISSRCROOT%\Files"
if errorlevel 1 goto failed
copy bzlib\Win32\Release\isbzip.dll "%ISSRCROOT%\Files"
if errorlevel 1 goto failed
copy bzlib\x64\Release\isbunzip-x64.dll "%ISSRCROOT%\Files"
if errorlevel 1 goto failed
copy bzlib\x64\Release\isbzip-x64.dll "%ISSRCROOT%\Files"
if errorlevel 1 goto failed

copy zlib\Win32\Release\isunzlib.dll "%ISSRCROOT%\Files"
if errorlevel 1 goto failed
copy zlib\Win32\Release\iszlib.dll "%ISSRCROOT%\Files"
if errorlevel 1 goto failed
copy zlib\x64\Release\isunzlib-x64.dll "%ISSRCROOT%\Files"
if errorlevel 1 goto failed
copy zlib\x64\Release\iszlib-x64.dll "%ISSRCROOT%\Files"
if errorlevel 1 goto failed

copy zstd\Win32\Release\isunzstd.dll "%ISSRCROOT%\Files"
if errorlevel 1 goto failed
copy zstd\Win32\Release\iszstd.dll "%ISSRCROOT%\Files"
if errorlevel 1 goto failed
copy zstd\x64\Release\isunzstd-x64.dll "%ISSRCROOT%\Files"
if errorlevel 1 goto failed
copy zstd\x64\Release\iszstd-x64.dll "%ISSRCROOT%\Files"
if errorlevel 1 goto failed

copy zstd\ARM64EC\Release\iszstd-Arm64EC.dll "%ISSRCROOT%\Files"
if errorlevel 1 goto failed

if "%1"=="nosynch" goto nosynch
if "%2"=="nosynch" goto nosynch
call "%ISSRCROOT%\Projects\Bin\synch-isfiles.bat" nopause
if errorlevel 1 goto failed
:nosynch

echo All done!
exit /b 0

:failed
echo *** FAILED ***
:failed2
exit /b 1