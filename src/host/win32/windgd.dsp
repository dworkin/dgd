# Microsoft Developer Studio Project File - Name="windgd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=windgd - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "windgd.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "windgd.mak" CFG="windgd - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "windgd - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "windgd - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "windgd - Win32 Release"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\Release"
# PROP Intermediate_Dir ".\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /Zp4 /MT /W3 /GX /Ox /Ot /Og /Oi /Ob2 /Gf /Gy /I "..\.." /I "..\..\comp" /I "..\..\lex" /I "..\..\ed" /I "..\..\kfun" /I "..\..\parser" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /FD /c
# SUBTRACT CPP /Ow /YX /Yc /Yu
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 ws2_32.lib nafxcw.lib /nologo /subsystem:windows /machine:I386 /out:".\Release\dgd.exe"

!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\Debug"
# PROP Intermediate_Dir ".\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /Zp4 /MTd /W3 /Gm /GX /ZI /Od /I "..\.." /I "..\..\comp" /I "..\..\lex" /I "..\..\ed" /I "..\..\kfun" /I "..\..\parser" /D "WIN32" /D "DEBUG" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /FR /FD /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 ws2_32.lib /nologo /subsystem:windows /debug /machine:I386 /out:".\Debug\dgd.exe"

!ENDIF 

# Begin Target

# Name "windgd - Win32 Release"
# Name "windgd - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Source File

SOURCE=..\..\alloc.c
# End Source File
# Begin Source File

SOURCE=..\..\array.c
# End Source File
# Begin Source File

SOURCE=..\asn.c
# End Source File
# Begin Source File

SOURCE=..\..\ed\buffer.c
# End Source File
# Begin Source File

SOURCE=..\..\kfun\builtin.c
# End Source File
# Begin Source File

SOURCE=..\..\call_out.c
# End Source File
# Begin Source File

SOURCE=..\..\ed\cmdsub.c
# End Source File
# Begin Source File

SOURCE=..\..\comp\codegeni.c
# End Source File
# Begin Source File

SOURCE=..\..\comm.c
# End Source File
# Begin Source File

SOURCE=..\..\comp\compile.c
# End Source File
# Begin Source File

SOURCE=..\..\config.c
# End Source File
# Begin Source File

SOURCE=.\connect.c
# End Source File
# Begin Source File

SOURCE=..\..\comp\control.c
# End Source File
# Begin Source File

SOURCE=..\crypt.c
# End Source File
# Begin Source File

SOURCE=..\..\comp\csupport.c
# End Source File
# Begin Source File

SOURCE=..\..\data.c
# End Source File
# Begin Source File

SOURCE=..\..\kfun\debug.c
# End Source File
# Begin Source File

SOURCE=..\..\parser\dfa.c
# End Source File
# Begin Source File

SOURCE=..\..\dgd.c
# End Source File
# Begin Source File

SOURCE=.\dosfile.c
# End Source File
# Begin Source File

SOURCE=..\..\ed\edcmd.c
# End Source File
# Begin Source File

SOURCE=..\..\editor.c
# End Source File
# Begin Source File

SOURCE=..\..\error.c
# End Source File
# Begin Source File

SOURCE=..\..\kfun\extra.c
# End Source File
# Begin Source File

SOURCE=..\..\kfun\file.c
# End Source File
# Begin Source File

SOURCE=..\..\ed\fileio.c
# End Source File
# Begin Source File

SOURCE=..\..\parser\grammar.c
# End Source File
# Begin Source File

SOURCE=..\..\hash.c
# End Source File
# Begin Source File

SOURCE=..\..\interpret.c
# End Source File
# Begin Source File

SOURCE=..\..\ed\line.c
# End Source File
# Begin Source File

SOURCE=.\local.c
# End Source File
# Begin Source File

SOURCE=..\..\lpc\lpc.c
# End Source File
# Begin Source File

SOURCE=..\..\lex\macro.c
# End Source File
# Begin Source File

SOURCE=.\MainFrame.cpp
# End Source File
# Begin Source File

SOURCE=..\..\kfun\math.c
# End Source File
# Begin Source File

SOURCE=..\..\comp\node.c
# End Source File
# Begin Source File

SOURCE=..\..\object.c
# End Source File
# Begin Source File

SOURCE=..\..\comp\optimize.c
# End Source File
# Begin Source File

SOURCE=..\..\parser\parse.c
# End Source File
# Begin Source File

SOURCE=..\..\comp\parser.c
# End Source File
# Begin Source File

SOURCE=..\..\path.c
# End Source File
# Begin Source File

SOURCE=..\..\lex\ppcontrol.c
# End Source File
# Begin Source File

SOURCE=..\..\lex\ppstr.c
# End Source File
# Begin Source File

SOURCE=..\..\ed\regexp.c
# End Source File
# Begin Source File

SOURCE=..\..\sdata.c
# End Source File
# Begin Source File

SOURCE=..\simfloat.c
# End Source File
# Begin Source File

SOURCE=..\..\lex\special.c
# End Source File
# Begin Source File

SOURCE=..\..\parser\srp.c
# End Source File
# Begin Source File

SOURCE=..\..\kfun\std.c
# End Source File
# Begin Source File

SOURCE=..\..\str.c
# End Source File
# Begin Source File

SOURCE=..\..\swap.c
# End Source File
# Begin Source File

SOURCE=..\..\kfun\table.c
# End Source File
# Begin Source File

SOURCE=.\time.c
# End Source File
# Begin Source File

SOURCE=..\..\lex\token.c
# End Source File
# Begin Source File

SOURCE=..\..\ed\vars.c
# End Source File
# Begin Source File

SOURCE=.\windgd.cpp
# End Source File
# Begin Source File

SOURCE=.\windgd.rc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=..\..\alloc.h
# End Source File
# Begin Source File

SOURCE=..\..\array.h
# End Source File
# Begin Source File

SOURCE=..\..\asn.h
# End Source File
# Begin Source File

SOURCE=..\..\ed\buffer.h
# End Source File
# Begin Source File

SOURCE=..\..\call_out.h
# End Source File
# Begin Source File

SOURCE=..\..\comp\codegen.h
# End Source File
# Begin Source File

SOURCE=..\..\comm.h
# End Source File
# Begin Source File

SOURCE=..\..\comp\comp.h
# End Source File
# Begin Source File

SOURCE=..\..\comp\compile.h
# End Source File
# Begin Source File

SOURCE=..\..\config.h
# End Source File
# Begin Source File

SOURCE=..\..\comp\control.h
# End Source File
# Begin Source File

SOURCE=..\..\comp\csupport.h
# End Source File
# Begin Source File

SOURCE=..\..\data.h
# End Source File
# Begin Source File

SOURCE=..\..\parser\dfa.h
# End Source File
# Begin Source File

SOURCE=..\..\dgd.h
# End Source File
# Begin Source File

SOURCE=..\..\ed\ed.h
# End Source File
# Begin Source File

SOURCE=..\..\ed\edcmd.h
# End Source File
# Begin Source File

SOURCE=..\..\editor.h
# End Source File
# Begin Source File

SOURCE=..\..\ed\fileio.h
# End Source File
# Begin Source File

SOURCE=..\..\parser\grammar.h
# End Source File
# Begin Source File

SOURCE=..\..\hash.h
# End Source File
# Begin Source File

SOURCE=..\..\host.h
# End Source File
# Begin Source File

SOURCE=..\..\interpret.h
# End Source File
# Begin Source File

SOURCE=..\..\kfun\kfun.h
# End Source File
# Begin Source File

SOURCE=..\..\lex\lex.h
# End Source File
# Begin Source File

SOURCE=..\..\ed\line.h
# End Source File
# Begin Source File

SOURCE=..\..\lex\macro.h
# End Source File
# Begin Source File

SOURCE=.\MainFrame.h
# End Source File
# Begin Source File

SOURCE=..\..\comp\node.h
# End Source File
# Begin Source File

SOURCE=..\..\object.h
# End Source File
# Begin Source File

SOURCE=..\..\comp\optimize.h
# End Source File
# Begin Source File

SOURCE=..\..\parser\parse.h
# End Source File
# Begin Source File

SOURCE=..\..\comp\parser.h
# End Source File
# Begin Source File

SOURCE=..\..\path.h
# End Source File
# Begin Source File

SOURCE=..\..\lex\ppcontrol.h
# End Source File
# Begin Source File

SOURCE=..\..\lex\ppstr.h
# End Source File
# Begin Source File

SOURCE=..\..\ed\regexp.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=..\..\lex\special.h
# End Source File
# Begin Source File

SOURCE=..\..\parser\srp.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=..\..\str.h
# End Source File
# Begin Source File

SOURCE=..\..\swap.h
# End Source File
# Begin Source File

SOURCE=..\..\kfun\table.h
# End Source File
# Begin Source File

SOURCE=..\telnet.h
# End Source File
# Begin Source File

SOURCE=..\..\lex\token.h
# End Source File
# Begin Source File

SOURCE=..\..\ed\vars.h
# End Source File
# Begin Source File

SOURCE=.\windgd.h
# End Source File
# Begin Source File

SOURCE=..\..\xfloat.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\windgd.ico
# End Source File
# Begin Source File

SOURCE=.\res\windgd.rc2
# End Source File
# Begin Source File

SOURCE=.\res\windgdDoc.ico
# End Source File
# End Group
# End Target
# End Project
