# Microsoft Developer Studio Project File - Name="mp3gain" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mp3gain - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mp3gain.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mp3gain.mak" CFG="mp3gain - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mp3gain - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mp3gain - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/analysis", SCAAAAAA"
# PROP Scc_LocalPath "."
CPP=xicl6.exe
RSC=rc.exe

!IF  "$(CFG)" == "mp3gain - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "HAVE_MEMCPY" /D "NOANALYSIS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "mp3gain - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "HAVE_MEMCPY" /D "NOANALYSIS" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "mp3gain - Win32 Release"
# Name "mp3gain - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter ".c"
# Begin Source File

SOURCE=.\mpglibDBL\common.c
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\dct64_i386.c
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\decode_i386.c
# End Source File
# Begin Source File

SOURCE=.\gain_analysis.c
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\interface.c
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\layer1.c
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\layer2.c
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\layer3.c
# End Source File
# Begin Source File

SOURCE=.\mp3gain.c
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\tabinit.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter ".h"
# Begin Source File

SOURCE=.\mpglibDBL\bitstream.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\common.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\config.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\dct64_i386.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\decode_i386.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\encoder.h
# End Source File
# Begin Source File

SOURCE=.\gain_analysis.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\huffman.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\interface.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\l2tables.h
# End Source File
# Begin Source File

SOURCE=".\mpglibDBL\lame-analysis.h"
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\lame.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\layer1.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\layer2.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\layer3.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\machine.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\mpg123.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\mpglib.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\tabinit.h
# End Source File
# Begin Source File

SOURCE=.\mpglibDBL\VbrTag.h
# End Source File
# End Group
# End Target
# End Project
