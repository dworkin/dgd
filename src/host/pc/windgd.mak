# Microsoft Developer Studio Generated NMAKE File, Format Version 40001
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

!IF "$(CFG)" == ""
CFG=windgd - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to windgd - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "windgd - Win32 Release" && "$(CFG)" != "windgd - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "windgd.mak" CFG="windgd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "windgd - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "windgd - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "windgd - Win32 Debug"
CPP=cl.exe
RSC=rc.exe
MTL=mktyplib.exe

!IF  "$(CFG)" == "windgd - Win32 Release"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
OUTDIR=.\Release
INTDIR=.\Release

ALL : "$(OUTDIR)\dgd.exe"

CLEAN : 
	-@erase ".\Release\dgd.exe"
	-@erase ".\Release\path.obj"
	-@erase ".\Release\node.obj"
	-@erase ".\Release\parse.obj"
	-@erase ".\Release\special.obj"
	-@erase ".\Release\config.obj"
	-@erase ".\Release\ppcontrol.obj"
	-@erase ".\Release\swap.obj"
	-@erase ".\Release\comm.obj"
	-@erase ".\Release\debug.obj"
	-@erase ".\Release\dfa.obj"
	-@erase ".\Release\str.obj"
	-@erase ".\Release\srp.obj"
	-@erase ".\Release\local.obj"
	-@erase ".\Release\dgd.obj"
	-@erase ".\Release\object.obj"
	-@erase ".\Release\simfloat.obj"
	-@erase ".\Release\std.obj"
	-@erase ".\Release\array.obj"
	-@erase ".\Release\fileio.obj"
	-@erase ".\Release\csupport.obj"
	-@erase ".\Release\crypt.obj"
	-@erase ".\Release\token.obj"
	-@erase ".\Release\codegeni.obj"
	-@erase ".\Release\alloc.obj"
	-@erase ".\Release\hash.obj"
	-@erase ".\Release\control.obj"
	-@erase ".\Release\ppstr.obj"
	-@erase ".\Release\regexp.obj"
	-@erase ".\Release\lpc.obj"
	-@erase ".\Release\data.obj"
	-@erase ".\Release\windgd.obj"
	-@erase ".\Release\optimize.obj"
	-@erase ".\Release\table.obj"
	-@erase ".\Release\extra.obj"
	-@erase ".\Release\vars.obj"
	-@erase ".\Release\edcmd.obj"
	-@erase ".\Release\cmdsub.obj"
	-@erase ".\Release\connect.obj"
	-@erase ".\Release\editor.obj"
	-@erase ".\Release\line.obj"
	-@erase ".\Release\grammar.obj"
	-@erase ".\Release\builtin.obj"
	-@erase ".\Release\call_out.obj"
	-@erase ".\Release\compile.obj"
	-@erase ".\Release\MainFrame.obj"
	-@erase ".\Release\macro.obj"
	-@erase ".\Release\error.obj"
	-@erase ".\Release\dosfile.obj"
	-@erase ".\Release\time.obj"
	-@erase ".\Release\file.obj"
	-@erase ".\Release\math.obj"
	-@erase ".\Release\buffer.obj"
	-@erase ".\Release\parser.obj"
	-@erase ".\Release\interpret.obj"
	-@erase ".\Release\windgd.res"
	-@erase ".\Release\sdata.obj"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /Zp4 /MT /W3 /GX /Ox /Ot /Og /Oi /Ob2 /Gf /Gy /I ".\..\.." /I ".\..\..\comp" /I ".\..\..\lex" /I ".\..\..\ed" /I ".\..\..\kfun" /I ".\..\..\parser" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /c
# SUBTRACT CPP /Ow /YX /Yc /Yu
CPP_PROJ=/nologo /Zp4 /MT /W3 /GX /Ox /Ot /Og /Oi /Ob2 /Gf /Gy /I ".\..\.." /I\
 ".\..\..\comp" /I ".\..\..\lex" /I ".\..\..\ed" /I ".\..\..\kfun" /I\
 ".\..\..\parser" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS"\
 /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/windgd.res" /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/windgd.bsc" 
BSC32_SBRS=
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 nafxcw.lib /nologo /subsystem:windows /machine:I386 /out:"Release/dgd.exe"
LINK32_FLAGS=nafxcw.lib /nologo /subsystem:windows /incremental:no\
 /pdb:"$(OUTDIR)/dgd.pdb" /machine:I386 /out:"$(OUTDIR)/dgd.exe" 
LINK32_OBJS= \
	".\Release\path.obj" \
	".\Release\node.obj" \
	".\Release\parse.obj" \
	".\Release\special.obj" \
	".\Release\config.obj" \
	".\Release\ppcontrol.obj" \
	".\Release\swap.obj" \
	".\Release\comm.obj" \
	".\Release\debug.obj" \
	".\Release\dfa.obj" \
	".\Release\str.obj" \
	".\Release\srp.obj" \
	".\Release\local.obj" \
	".\Release\dgd.obj" \
	".\Release\object.obj" \
	".\Release\simfloat.obj" \
	".\Release\std.obj" \
	".\Release\array.obj" \
	".\Release\fileio.obj" \
	".\Release\csupport.obj" \
	".\Release\crypt.obj" \
	".\Release\token.obj" \
	".\Release\codegeni.obj" \
	".\Release\alloc.obj" \
	".\Release\hash.obj" \
	".\Release\control.obj" \
	".\Release\ppstr.obj" \
	".\Release\regexp.obj" \
	".\Release\lpc.obj" \
	".\Release\data.obj" \
	".\Release\windgd.obj" \
	".\Release\optimize.obj" \
	".\Release\table.obj" \
	".\Release\extra.obj" \
	".\Release\vars.obj" \
	".\Release\edcmd.obj" \
	".\Release\cmdsub.obj" \
	".\Release\connect.obj" \
	".\Release\editor.obj" \
	".\Release\line.obj" \
	".\Release\grammar.obj" \
	".\Release\builtin.obj" \
	".\Release\call_out.obj" \
	".\Release\compile.obj" \
	".\Release\MainFrame.obj" \
	".\Release\macro.obj" \
	".\Release\error.obj" \
	".\Release\dosfile.obj" \
	".\Release\time.obj" \
	".\Release\file.obj" \
	".\Release\math.obj" \
	".\Release\buffer.obj" \
	".\Release\parser.obj" \
	".\Release\interpret.obj" \
	".\Release\sdata.obj" \
	".\Release\windgd.res"

"$(OUTDIR)\dgd.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : "$(OUTDIR)\dgd.exe" "$(OUTDIR)\windgd.bsc"

CLEAN : 
	-@erase ".\Debug\vc40.pdb"
	-@erase ".\Debug\vc40.idb"
	-@erase ".\Debug\windgd.bsc"
	-@erase ".\Debug\call_out.sbr"
	-@erase ".\Debug\vars.sbr"
	-@erase ".\Debug\cmdsub.sbr"
	-@erase ".\Debug\debug.sbr"
	-@erase ".\Debug\local.sbr"
	-@erase ".\Debug\macro.sbr"
	-@erase ".\Debug\error.sbr"
	-@erase ".\Debug\editor.sbr"
	-@erase ".\Debug\control.sbr"
	-@erase ".\Debug\line.sbr"
	-@erase ".\Debug\simfloat.sbr"
	-@erase ".\Debug\interpret.sbr"
	-@erase ".\Debug\token.sbr"
	-@erase ".\Debug\csupport.sbr"
	-@erase ".\Debug\codegeni.sbr"
	-@erase ".\Debug\alloc.sbr"
	-@erase ".\Debug\connect.sbr"
	-@erase ".\Debug\time.sbr"
	-@erase ".\Debug\parse.sbr"
	-@erase ".\Debug\ppcontrol.sbr"
	-@erase ".\Debug\grammar.sbr"
	-@erase ".\Debug\optimize.sbr"
	-@erase ".\Debug\builtin.sbr"
	-@erase ".\Debug\compile.sbr"
	-@erase ".\Debug\MainFrame.sbr"
	-@erase ".\Debug\buffer.sbr"
	-@erase ".\Debug\dfa.sbr"
	-@erase ".\Debug\math.sbr"
	-@erase ".\Debug\str.sbr"
	-@erase ".\Debug\srp.sbr"
	-@erase ".\Debug\parser.sbr"
	-@erase ".\Debug\dgd.sbr"
	-@erase ".\Debug\std.sbr"
	-@erase ".\Debug\dosfile.sbr"
	-@erase ".\Debug\path.sbr"
	-@erase ".\Debug\node.sbr"
	-@erase ".\Debug\config.sbr"
	-@erase ".\Debug\array.sbr"
	-@erase ".\Debug\crypt.sbr"
	-@erase ".\Debug\lpc.sbr"
	-@erase ".\Debug\swap.sbr"
	-@erase ".\Debug\comm.sbr"
	-@erase ".\Debug\ppstr.sbr"
	-@erase ".\Debug\object.sbr"
	-@erase ".\Debug\table.sbr"
	-@erase ".\Debug\extra.sbr"
	-@erase ".\Debug\fileio.sbr"
	-@erase ".\Debug\special.sbr"
	-@erase ".\Debug\edcmd.sbr"
	-@erase ".\Debug\regexp.sbr"
	-@erase ".\Debug\hash.sbr"
	-@erase ".\Debug\file.sbr"
	-@erase ".\Debug\windgd.sbr"
	-@erase ".\Debug\data.sbr"
	-@erase ".\Debug\sdata.sbr"
	-@erase ".\Debug\dgd.exe"
	-@erase ".\Debug\table.obj"
	-@erase ".\Debug\extra.obj"
	-@erase ".\Debug\fileio.obj"
	-@erase ".\Debug\special.obj"
	-@erase ".\Debug\edcmd.obj"
	-@erase ".\Debug\regexp.obj"
	-@erase ".\Debug\hash.obj"
	-@erase ".\Debug\file.obj"
	-@erase ".\Debug\windgd.obj"
	-@erase ".\Debug\data.obj"
	-@erase ".\Debug\call_out.obj"
	-@erase ".\Debug\vars.obj"
	-@erase ".\Debug\cmdsub.obj"
	-@erase ".\Debug\debug.obj"
	-@erase ".\Debug\local.obj"
	-@erase ".\Debug\macro.obj"
	-@erase ".\Debug\error.obj"
	-@erase ".\Debug\editor.obj"
	-@erase ".\Debug\control.obj"
	-@erase ".\Debug\line.obj"
	-@erase ".\Debug\simfloat.obj"
	-@erase ".\Debug\interpret.obj"
	-@erase ".\Debug\token.obj"
	-@erase ".\Debug\csupport.obj"
	-@erase ".\Debug\codegeni.obj"
	-@erase ".\Debug\alloc.obj"
	-@erase ".\Debug\connect.obj"
	-@erase ".\Debug\time.obj"
	-@erase ".\Debug\parse.obj"
	-@erase ".\Debug\ppcontrol.obj"
	-@erase ".\Debug\grammar.obj"
	-@erase ".\Debug\optimize.obj"
	-@erase ".\Debug\builtin.obj"
	-@erase ".\Debug\compile.obj"
	-@erase ".\Debug\MainFrame.obj"
	-@erase ".\Debug\buffer.obj"
	-@erase ".\Debug\dfa.obj"
	-@erase ".\Debug\math.obj"
	-@erase ".\Debug\str.obj"
	-@erase ".\Debug\srp.obj"
	-@erase ".\Debug\parser.obj"
	-@erase ".\Debug\dgd.obj"
	-@erase ".\Debug\std.obj"
	-@erase ".\Debug\dosfile.obj"
	-@erase ".\Debug\path.obj"
	-@erase ".\Debug\node.obj"
	-@erase ".\Debug\config.obj"
	-@erase ".\Debug\array.obj"
	-@erase ".\Debug\crypt.obj"
	-@erase ".\Debug\lpc.obj"
	-@erase ".\Debug\swap.obj"
	-@erase ".\Debug\comm.obj"
	-@erase ".\Debug\ppstr.obj"
	-@erase ".\Debug\object.obj"
	-@erase ".\Debug\windgd.res"
	-@erase ".\Debug\sdata.obj"
	-@erase ".\Debug\dgd.ilk"
	-@erase ".\Debug\dgd.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /Zp4 /MTd /W3 /Gm /GX /Zi /Od /I ".\..\.." /I ".\..\..\comp" /I ".\..\..\lex" /I ".\..\..\ed" /I ".\..\..\kfun" /I ".\..\..\parser" /D "WIN32" /D "DEBUG" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /FR /c
# SUBTRACT CPP /YX /Yc /Yu
CPP_PROJ=/nologo /Zp4 /MTd /W3 /Gm /GX /Zi /Od /I ".\..\.." /I ".\..\..\comp"\
 /I ".\..\..\lex" /I ".\..\..\ed" /I ".\..\..\kfun" /I ".\..\..\parser" /D\
 "WIN32" /D "DEBUG" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /FR"$(INTDIR)/"\
 /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\Debug/
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/windgd.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/windgd.bsc" 
BSC32_SBRS= \
	".\Debug\call_out.sbr" \
	".\Debug\vars.sbr" \
	".\Debug\cmdsub.sbr" \
	".\Debug\debug.sbr" \
	".\Debug\local.sbr" \
	".\Debug\macro.sbr" \
	".\Debug\error.sbr" \
	".\Debug\editor.sbr" \
	".\Debug\control.sbr" \
	".\Debug\line.sbr" \
	".\Debug\simfloat.sbr" \
	".\Debug\interpret.sbr" \
	".\Debug\token.sbr" \
	".\Debug\csupport.sbr" \
	".\Debug\codegeni.sbr" \
	".\Debug\alloc.sbr" \
	".\Debug\connect.sbr" \
	".\Debug\time.sbr" \
	".\Debug\parse.sbr" \
	".\Debug\ppcontrol.sbr" \
	".\Debug\grammar.sbr" \
	".\Debug\optimize.sbr" \
	".\Debug\builtin.sbr" \
	".\Debug\compile.sbr" \
	".\Debug\MainFrame.sbr" \
	".\Debug\buffer.sbr" \
	".\Debug\dfa.sbr" \
	".\Debug\math.sbr" \
	".\Debug\str.sbr" \
	".\Debug\srp.sbr" \
	".\Debug\parser.sbr" \
	".\Debug\dgd.sbr" \
	".\Debug\std.sbr" \
	".\Debug\dosfile.sbr" \
	".\Debug\path.sbr" \
	".\Debug\node.sbr" \
	".\Debug\config.sbr" \
	".\Debug\array.sbr" \
	".\Debug\crypt.sbr" \
	".\Debug\lpc.sbr" \
	".\Debug\swap.sbr" \
	".\Debug\comm.sbr" \
	".\Debug\ppstr.sbr" \
	".\Debug\object.sbr" \
	".\Debug\table.sbr" \
	".\Debug\extra.sbr" \
	".\Debug\fileio.sbr" \
	".\Debug\special.sbr" \
	".\Debug\edcmd.sbr" \
	".\Debug\regexp.sbr" \
	".\Debug\hash.sbr" \
	".\Debug\file.sbr" \
	".\Debug\windgd.sbr" \
	".\Debug\data.sbr" \
	".\Debug\sdata.sbr"

"$(OUTDIR)\windgd.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 /nologo /subsystem:windows /debug /machine:I386 /out:"Debug/dgd.exe"
LINK32_FLAGS=/nologo /subsystem:windows /incremental:yes\
 /pdb:"$(OUTDIR)/dgd.pdb" /debug /machine:I386 /out:"$(OUTDIR)/dgd.exe" 
LINK32_OBJS= \
	".\Debug\table.obj" \
	".\Debug\extra.obj" \
	".\Debug\fileio.obj" \
	".\Debug\special.obj" \
	".\Debug\edcmd.obj" \
	".\Debug\regexp.obj" \
	".\Debug\hash.obj" \
	".\Debug\file.obj" \
	".\Debug\windgd.obj" \
	".\Debug\data.obj" \
	".\Debug\call_out.obj" \
	".\Debug\vars.obj" \
	".\Debug\cmdsub.obj" \
	".\Debug\debug.obj" \
	".\Debug\local.obj" \
	".\Debug\macro.obj" \
	".\Debug\error.obj" \
	".\Debug\editor.obj" \
	".\Debug\control.obj" \
	".\Debug\line.obj" \
	".\Debug\simfloat.obj" \
	".\Debug\interpret.obj" \
	".\Debug\token.obj" \
	".\Debug\csupport.obj" \
	".\Debug\codegeni.obj" \
	".\Debug\alloc.obj" \
	".\Debug\connect.obj" \
	".\Debug\time.obj" \
	".\Debug\parse.obj" \
	".\Debug\ppcontrol.obj" \
	".\Debug\grammar.obj" \
	".\Debug\optimize.obj" \
	".\Debug\builtin.obj" \
	".\Debug\compile.obj" \
	".\Debug\MainFrame.obj" \
	".\Debug\buffer.obj" \
	".\Debug\dfa.obj" \
	".\Debug\math.obj" \
	".\Debug\str.obj" \
	".\Debug\srp.obj" \
	".\Debug\parser.obj" \
	".\Debug\dgd.obj" \
	".\Debug\std.obj" \
	".\Debug\dosfile.obj" \
	".\Debug\path.obj" \
	".\Debug\node.obj" \
	".\Debug\config.obj" \
	".\Debug\array.obj" \
	".\Debug\crypt.obj" \
	".\Debug\lpc.obj" \
	".\Debug\swap.obj" \
	".\Debug\comm.obj" \
	".\Debug\ppstr.obj" \
	".\Debug\object.obj" \
	".\Debug\sdata.obj" \
	".\Debug\windgd.res"

"$(OUTDIR)\dgd.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "windgd - Win32 Release"
# Name "windgd - Win32 Debug"

!IF  "$(CFG)" == "windgd - Win32 Release"

!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\windgd.cpp

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_WINDG=\
	".\StdAfx.h"\
	".\MainFrame.h"\
	".\windgd.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_WINDG=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\windgd.obj" : $(SOURCE) $(DEP_CPP_WINDG) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_WINDG=\
	".\StdAfx.h"\
	".\MainFrame.h"\
	".\windgd.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

"$(INTDIR)\windgd.obj" : $(SOURCE) $(DEP_CPP_WINDG) "$(INTDIR)"

"$(INTDIR)\windgd.sbr" : $(SOURCE) $(DEP_CPP_WINDG) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\windgd.rc
DEP_RSC_WINDGD=\
	".\res\windgd.ico"\
	".\res\windgdDoc.ico"\
	".\res\windgd.rc2"\
	

"$(INTDIR)\windgd.res" : $(SOURCE) $(DEP_RSC_WINDGD) "$(INTDIR)"
   $(RSC) $(RSC_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\swap.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_SWAP_=\
	".\..\..\dgd.h"\
	".\..\..\swap.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_SWAP_=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\swap.obj" : $(SOURCE) $(DEP_CPP_SWAP_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_SWAP_=\
	".\..\..\dgd.h"\
	".\..\..\swap.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\swap.obj" : $(SOURCE) $(DEP_CPP_SWAP_) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\swap.sbr" : $(SOURCE) $(DEP_CPP_SWAP_) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\array.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_ARRAY=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_ARRAY=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\array.obj" : $(SOURCE) $(DEP_CPP_ARRAY) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_ARRAY=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\array.obj" : $(SOURCE) $(DEP_CPP_ARRAY) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\array.sbr" : $(SOURCE) $(DEP_CPP_ARRAY) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\call_out.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_CALL_=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\call_out.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_CALL_=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\call_out.obj" : $(SOURCE) $(DEP_CPP_CALL_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_CALL_=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\call_out.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\call_out.obj" : $(SOURCE) $(DEP_CPP_CALL_) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\call_out.sbr" : $(SOURCE) $(DEP_CPP_CALL_) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\comm.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_COMM_=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\comm.h"\
	".\..\..\version.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_COMM_=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\comm.obj" : $(SOURCE) $(DEP_CPP_COMM_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_COMM_=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\comm.h"\
	".\..\..\version.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\comm.obj" : $(SOURCE) $(DEP_CPP_COMM_) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\comm.sbr" : $(SOURCE) $(DEP_CPP_COMM_) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\config.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_CONFI=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\path.h"\
	".\..\..\editor.h"\
	".\..\..\call_out.h"\
	".\..\..\comm.h"\
	".\..\..\version.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\token.h"\
	".\..\..\lex\ppcontrol.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\parser.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\csupport.h"\
	".\..\..\kfun\table.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_CONFI=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\config.obj" : $(SOURCE) $(DEP_CPP_CONFI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_CONFI=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\path.h"\
	".\..\..\editor.h"\
	".\..\..\call_out.h"\
	".\..\..\comm.h"\
	".\..\..\version.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\token.h"\
	".\..\..\lex\ppcontrol.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\parser.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\csupport.h"\
	".\..\..\kfun\table.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\config.obj" : $(SOURCE) $(DEP_CPP_CONFI) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\config.sbr" : $(SOURCE) $(DEP_CPP_CONFI) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\data.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_DATA_=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\call_out.h"\
	".\..\..\parser\parse.h"\
	".\..\..\comp\csupport.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_DATA_=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\data.obj" : $(SOURCE) $(DEP_CPP_DATA_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_DATA_=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\call_out.h"\
	".\..\..\parser\parse.h"\
	".\..\..\comp\csupport.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\data.obj" : $(SOURCE) $(DEP_CPP_DATA_) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\data.sbr" : $(SOURCE) $(DEP_CPP_DATA_) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\dgd.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_DGD_C=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\editor.h"\
	".\..\..\call_out.h"\
	".\..\..\comm.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_DGD_C=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\dgd.obj" : $(SOURCE) $(DEP_CPP_DGD_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_DGD_C=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\editor.h"\
	".\..\..\call_out.h"\
	".\..\..\comm.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\dgd.obj" : $(SOURCE) $(DEP_CPP_DGD_C) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\dgd.sbr" : $(SOURCE) $(DEP_CPP_DGD_C) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\editor.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_EDITO=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\ed\edcmd.h"\
	".\..\..\editor.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	".\..\..\ed\regexp.h"\
	".\..\..\ed\buffer.h"\
	".\..\..\ed\vars.h"\
	".\..\..\ed\line.h"\
	
NODEP_CPP_EDITO=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\editor.obj" : $(SOURCE) $(DEP_CPP_EDITO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_EDITO=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\ed\edcmd.h"\
	".\..\..\editor.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	".\..\..\ed\regexp.h"\
	".\..\..\ed\buffer.h"\
	".\..\..\ed\vars.h"\
	".\..\..\ed\line.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\editor.obj" : $(SOURCE) $(DEP_CPP_EDITO) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\editor.sbr" : $(SOURCE) $(DEP_CPP_EDITO) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\error.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_ERROR=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\data.h"\
	".\..\..\interpret.h"\
	".\..\..\comm.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_ERROR=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\error.obj" : $(SOURCE) $(DEP_CPP_ERROR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_ERROR=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\data.h"\
	".\..\..\interpret.h"\
	".\..\..\comm.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\error.obj" : $(SOURCE) $(DEP_CPP_ERROR) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\error.sbr" : $(SOURCE) $(DEP_CPP_ERROR) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\hash.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_HASH_=\
	".\..\..\dgd.h"\
	".\..\..\hash.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_HASH_=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\hash.obj" : $(SOURCE) $(DEP_CPP_HASH_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_HASH_=\
	".\..\..\dgd.h"\
	".\..\..\hash.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\hash.obj" : $(SOURCE) $(DEP_CPP_HASH_) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\hash.sbr" : $(SOURCE) $(DEP_CPP_HASH_) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\interpret.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_INTER=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\csupport.h"\
	".\..\..\kfun\table.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_INTER=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\interpret.obj" : $(SOURCE) $(DEP_CPP_INTER) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_INTER=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\csupport.h"\
	".\..\..\kfun\table.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\interpret.obj" : $(SOURCE) $(DEP_CPP_INTER) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\interpret.sbr" : $(SOURCE) $(DEP_CPP_INTER) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\object.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_OBJEC=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_OBJEC=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\object.obj" : $(SOURCE) $(DEP_CPP_OBJEC) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_OBJEC=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\object.obj" : $(SOURCE) $(DEP_CPP_OBJEC) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\object.sbr" : $(SOURCE) $(DEP_CPP_OBJEC) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\path.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_PATH_=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\path.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_PATH_=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\path.obj" : $(SOURCE) $(DEP_CPP_PATH_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_PATH_=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\path.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\path.obj" : $(SOURCE) $(DEP_CPP_PATH_) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\path.sbr" : $(SOURCE) $(DEP_CPP_PATH_) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\str.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_STR_C=\
	".\..\..\dgd.h"\
	".\..\..\hash.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_STR_C=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\str.obj" : $(SOURCE) $(DEP_CPP_STR_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_STR_C=\
	".\..\..\dgd.h"\
	".\..\..\hash.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\str.obj" : $(SOURCE) $(DEP_CPP_STR_C) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\str.sbr" : $(SOURCE) $(DEP_CPP_STR_C) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\alloc.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_ALLOC=\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_ALLOC=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\alloc.obj" : $(SOURCE) $(DEP_CPP_ALLOC) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_ALLOC=\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\alloc.obj" : $(SOURCE) $(DEP_CPP_ALLOC) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\alloc.sbr" : $(SOURCE) $(DEP_CPP_ALLOC) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\comp\codegeni.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_CODEG=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\kfun\table.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\codegen.h"\
	".\..\..\comp\compile.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_CODEG=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\codegeni.obj" : $(SOURCE) $(DEP_CPP_CODEG) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_CODEG=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\kfun\table.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\codegen.h"\
	".\..\..\comp\compile.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\codegeni.obj" : $(SOURCE) $(DEP_CPP_CODEG) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\codegeni.sbr" : $(SOURCE) $(DEP_CPP_CODEG) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\comp\compile.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_COMPI=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\path.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\token.h"\
	".\..\..\lex\ppcontrol.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\optimize.h"\
	".\..\..\comp\codegen.h"\
	".\..\..\comp\compile.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_COMPI=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\compile.obj" : $(SOURCE) $(DEP_CPP_COMPI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_COMPI=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\path.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\token.h"\
	".\..\..\lex\ppcontrol.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\optimize.h"\
	".\..\..\comp\codegen.h"\
	".\..\..\comp\compile.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\compile.obj" : $(SOURCE) $(DEP_CPP_COMPI) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\compile.sbr" : $(SOURCE) $(DEP_CPP_COMPI) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\comp\control.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_CONTR=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\hash.h"\
	".\..\..\kfun\table.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\control.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_CONTR=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\control.obj" : $(SOURCE) $(DEP_CPP_CONTR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_CONTR=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\hash.h"\
	".\..\..\kfun\table.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\control.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\control.obj" : $(SOURCE) $(DEP_CPP_CONTR) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\control.sbr" : $(SOURCE) $(DEP_CPP_CONTR) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\comp\csupport.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_CSUPP=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\kfun\table.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\csupport.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_CSUPP=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\csupport.obj" : $(SOURCE) $(DEP_CPP_CSUPP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_CSUPP=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\kfun\table.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\csupport.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\csupport.obj" : $(SOURCE) $(DEP_CPP_CSUPP) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\csupport.sbr" : $(SOURCE) $(DEP_CPP_CSUPP) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\comp\node.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_NODE_=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\token.h"\
	".\..\..\comp\node.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_NODE_=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\node.obj" : $(SOURCE) $(DEP_CPP_NODE_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_NODE_=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\token.h"\
	".\..\..\comp\node.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\node.obj" : $(SOURCE) $(DEP_CPP_NODE_) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\node.sbr" : $(SOURCE) $(DEP_CPP_NODE_) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\comp\optimize.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_OPTIM=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\kfun\table.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\optimize.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_OPTIM=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\optimize.obj" : $(SOURCE) $(DEP_CPP_OPTIM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_OPTIM=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\kfun\table.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\optimize.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\optimize.obj" : $(SOURCE) $(DEP_CPP_OPTIM) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\optimize.sbr" : $(SOURCE) $(DEP_CPP_OPTIM) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\comp\parser.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_PARSE=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\token.h"\
	".\..\..\lex\ppcontrol.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_PARSE=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\parser.obj" : $(SOURCE) $(DEP_CPP_PARSE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_PARSE=\
	".\..\..\comp\comp.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\token.h"\
	".\..\..\lex\ppcontrol.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\parser.obj" : $(SOURCE) $(DEP_CPP_PARSE) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\parser.sbr" : $(SOURCE) $(DEP_CPP_PARSE) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\ed\buffer.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_BUFFE=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\buffer.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\ed\line.h"\
	
NODEP_CPP_BUFFE=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\buffer.obj" : $(SOURCE) $(DEP_CPP_BUFFE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_BUFFE=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\buffer.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\ed\line.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\buffer.obj" : $(SOURCE) $(DEP_CPP_BUFFE) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\buffer.sbr" : $(SOURCE) $(DEP_CPP_BUFFE) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\ed\cmdsub.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_CMDSU=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\edcmd.h"\
	".\..\..\ed\fileio.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\ed\regexp.h"\
	".\..\..\ed\buffer.h"\
	".\..\..\ed\vars.h"\
	".\..\..\ed\line.h"\
	
NODEP_CPP_CMDSU=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\cmdsub.obj" : $(SOURCE) $(DEP_CPP_CMDSU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_CMDSU=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\edcmd.h"\
	".\..\..\ed\fileio.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\ed\regexp.h"\
	".\..\..\ed\buffer.h"\
	".\..\..\ed\vars.h"\
	".\..\..\ed\line.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\cmdsub.obj" : $(SOURCE) $(DEP_CPP_CMDSU) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\cmdsub.sbr" : $(SOURCE) $(DEP_CPP_CMDSU) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\ed\edcmd.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_EDCMD=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\edcmd.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\ed\regexp.h"\
	".\..\..\ed\buffer.h"\
	".\..\..\ed\vars.h"\
	".\..\..\ed\line.h"\
	
NODEP_CPP_EDCMD=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\edcmd.obj" : $(SOURCE) $(DEP_CPP_EDCMD) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_EDCMD=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\edcmd.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\ed\regexp.h"\
	".\..\..\ed\buffer.h"\
	".\..\..\ed\vars.h"\
	".\..\..\ed\line.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\edcmd.obj" : $(SOURCE) $(DEP_CPP_EDCMD) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\edcmd.sbr" : $(SOURCE) $(DEP_CPP_EDCMD) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\ed\fileio.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_FILEI=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\buffer.h"\
	".\..\..\path.h"\
	".\..\..\ed\fileio.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\ed\line.h"\
	
NODEP_CPP_FILEI=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\fileio.obj" : $(SOURCE) $(DEP_CPP_FILEI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_FILEI=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\buffer.h"\
	".\..\..\path.h"\
	".\..\..\ed\fileio.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\ed\line.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\fileio.obj" : $(SOURCE) $(DEP_CPP_FILEI) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\fileio.sbr" : $(SOURCE) $(DEP_CPP_FILEI) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\ed\line.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_LINE_=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\line.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_LINE_=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\line.obj" : $(SOURCE) $(DEP_CPP_LINE_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_LINE_=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\line.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\line.obj" : $(SOURCE) $(DEP_CPP_LINE_) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\line.sbr" : $(SOURCE) $(DEP_CPP_LINE_) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\ed\regexp.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_REGEX=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\regexp.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_REGEX=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\regexp.obj" : $(SOURCE) $(DEP_CPP_REGEX) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_REGEX=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\regexp.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\regexp.obj" : $(SOURCE) $(DEP_CPP_REGEX) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\regexp.sbr" : $(SOURCE) $(DEP_CPP_REGEX) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\ed\vars.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_VARS_=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\vars.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_VARS_=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\vars.obj" : $(SOURCE) $(DEP_CPP_VARS_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_VARS_=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\vars.h"\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\vars.obj" : $(SOURCE) $(DEP_CPP_VARS_) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\vars.sbr" : $(SOURCE) $(DEP_CPP_VARS_) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\host\simfloat.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_SIMFL=\
	".\..\..\dgd.h"\
	".\..\..\xfloat.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_SIMFL=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\simfloat.obj" : $(SOURCE) $(DEP_CPP_SIMFL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_SIMFL=\
	".\..\..\dgd.h"\
	".\..\..\xfloat.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\simfloat.obj" : $(SOURCE) $(DEP_CPP_SIMFL) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\simfloat.sbr" : $(SOURCE) $(DEP_CPP_SIMFL) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\host\crypt.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_CRYPT=\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_CRYPT=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\crypt.obj" : $(SOURCE) $(DEP_CPP_CRYPT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_CRYPT=\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\crypt.obj" : $(SOURCE) $(DEP_CPP_CRYPT) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\crypt.sbr" : $(SOURCE) $(DEP_CPP_CRYPT) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\kfun\table.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_TABLE=\
	".\..\..\kfun\kfun.h"\
	".\..\..\kfun\table.h"\
	".\..\..\kfun\builtin.c"\
	".\..\..\kfun\std.c"\
	".\..\..\kfun\file.c"\
	".\..\..\kfun\math.c"\
	".\..\..\kfun\extra.c"\
	".\..\..\kfun\debug.c"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	".\..\..\path.h"\
	".\..\..\comm.h"\
	".\..\..\call_out.h"\
	".\..\..\editor.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\compile.h"\
	".\..\..\parser\parse.h"\
	
NODEP_CPP_TABLE=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\table.obj" : $(SOURCE) $(DEP_CPP_TABLE) "$(INTDIR)"\
 ".\..\..\kfun\builtin.c" ".\..\..\kfun\std.c" ".\..\..\kfun\file.c"\
 ".\..\..\kfun\math.c" ".\..\..\kfun\extra.c" ".\..\..\kfun\debug.c"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_TABLE=\
	".\..\..\kfun\kfun.h"\
	".\..\..\kfun\table.h"\
	".\..\..\kfun\builtin.c"\
	".\..\..\kfun\std.c"\
	".\..\..\kfun\file.c"\
	".\..\..\kfun\math.c"\
	".\..\..\kfun\extra.c"\
	".\..\..\kfun\debug.c"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	".\..\..\path.h"\
	".\..\..\comm.h"\
	".\..\..\call_out.h"\
	".\..\..\editor.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\compile.h"\
	".\..\..\parser\parse.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\table.obj" : $(SOURCE) $(DEP_CPP_TABLE) "$(INTDIR)"\
 ".\..\..\kfun\builtin.c" ".\..\..\kfun\std.c" ".\..\..\kfun\file.c"\
 ".\..\..\kfun\math.c" ".\..\..\kfun\extra.c" ".\..\..\kfun\debug.c"
   $(BuildCmds)

"$(INTDIR)\table.sbr" : $(SOURCE) $(DEP_CPP_TABLE) "$(INTDIR)"\
 ".\..\..\kfun\builtin.c" ".\..\..\kfun\std.c" ".\..\..\kfun\file.c"\
 ".\..\..\kfun\math.c" ".\..\..\kfun\extra.c" ".\..\..\kfun\debug.c"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\kfun\debug.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_DEBUG=\
	".\..\..\kfun\kfun.h"\
	".\..\..\comp\control.h"\
	".\..\..\kfun\table.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_DEBUG=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\debug.obj" : $(SOURCE) $(DEP_CPP_DEBUG) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_DEBUG=\
	".\..\..\kfun\kfun.h"\
	".\..\..\comp\control.h"\
	".\..\..\kfun\table.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\debug.obj" : $(SOURCE) $(DEP_CPP_DEBUG) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\debug.sbr" : $(SOURCE) $(DEP_CPP_DEBUG) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\kfun\extra.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_EXTRA=\
	".\..\..\kfun\kfun.h"\
	".\..\..\parser\parse.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_EXTRA=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\extra.obj" : $(SOURCE) $(DEP_CPP_EXTRA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_EXTRA=\
	".\..\..\kfun\kfun.h"\
	".\..\..\parser\parse.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\extra.obj" : $(SOURCE) $(DEP_CPP_EXTRA) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\extra.sbr" : $(SOURCE) $(DEP_CPP_EXTRA) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\kfun\file.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_FILE_=\
	".\..\..\kfun\kfun.h"\
	".\..\..\path.h"\
	".\..\..\editor.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_FILE_=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\file.obj" : $(SOURCE) $(DEP_CPP_FILE_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_FILE_=\
	".\..\..\kfun\kfun.h"\
	".\..\..\path.h"\
	".\..\..\editor.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\file.obj" : $(SOURCE) $(DEP_CPP_FILE_) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\file.sbr" : $(SOURCE) $(DEP_CPP_FILE_) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\kfun\math.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_MATH_=\
	".\..\..\kfun\kfun.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_MATH_=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\math.obj" : $(SOURCE) $(DEP_CPP_MATH_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_MATH_=\
	".\..\..\kfun\kfun.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\math.obj" : $(SOURCE) $(DEP_CPP_MATH_) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\math.sbr" : $(SOURCE) $(DEP_CPP_MATH_) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\kfun\std.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_STD_C=\
	".\..\..\kfun\kfun.h"\
	".\..\..\path.h"\
	".\..\..\comm.h"\
	".\..\..\call_out.h"\
	".\..\..\editor.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\compile.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_STD_C=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\std.obj" : $(SOURCE) $(DEP_CPP_STD_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_STD_C=\
	".\..\..\kfun\kfun.h"\
	".\..\..\path.h"\
	".\..\..\comm.h"\
	".\..\..\call_out.h"\
	".\..\..\editor.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\control.h"\
	".\..\..\comp\compile.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\std.obj" : $(SOURCE) $(DEP_CPP_STD_C) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\std.sbr" : $(SOURCE) $(DEP_CPP_STD_C) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\kfun\builtin.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_BUILT=\
	".\..\..\kfun\kfun.h"\
	".\..\..\kfun\table.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_BUILT=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\builtin.obj" : $(SOURCE) $(DEP_CPP_BUILT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_BUILT=\
	".\..\..\kfun\kfun.h"\
	".\..\..\kfun\table.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\xfloat.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\builtin.obj" : $(SOURCE) $(DEP_CPP_BUILT) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\builtin.sbr" : $(SOURCE) $(DEP_CPP_BUILT) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\lex\token.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_TOKEN=\
	".\..\..\lex\lex.h"\
	".\..\..\path.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\special.h"\
	".\..\..\lex\ppstr.h"\
	".\..\..\lex\token.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\xfloat.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\parser.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	
NODEP_CPP_TOKEN=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\token.obj" : $(SOURCE) $(DEP_CPP_TOKEN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_TOKEN=\
	".\..\..\lex\lex.h"\
	".\..\..\path.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\special.h"\
	".\..\..\lex\ppstr.h"\
	".\..\..\lex\token.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\xfloat.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\parser.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\token.obj" : $(SOURCE) $(DEP_CPP_TOKEN) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\token.sbr" : $(SOURCE) $(DEP_CPP_TOKEN) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\lex\ppcontrol.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_PPCON=\
	".\..\..\lex\lex.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\special.h"\
	".\..\..\lex\ppstr.h"\
	".\..\..\lex\token.h"\
	".\..\..\path.h"\
	".\..\..\lex\ppcontrol.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\xfloat.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\parser.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	
NODEP_CPP_PPCON=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\ppcontrol.obj" : $(SOURCE) $(DEP_CPP_PPCON) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_PPCON=\
	".\..\..\lex\lex.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\special.h"\
	".\..\..\lex\ppstr.h"\
	".\..\..\lex\token.h"\
	".\..\..\path.h"\
	".\..\..\lex\ppcontrol.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\xfloat.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\parser.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\ppcontrol.obj" : $(SOURCE) $(DEP_CPP_PPCON) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\ppcontrol.sbr" : $(SOURCE) $(DEP_CPP_PPCON) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\lex\ppstr.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_PPSTR=\
	".\..\..\lex\lex.h"\
	".\..\..\lex\ppstr.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\xfloat.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\parser.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_PPSTR=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\ppstr.obj" : $(SOURCE) $(DEP_CPP_PPSTR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_PPSTR=\
	".\..\..\lex\lex.h"\
	".\..\..\lex\ppstr.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\xfloat.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\parser.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\ppstr.obj" : $(SOURCE) $(DEP_CPP_PPSTR) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\ppstr.sbr" : $(SOURCE) $(DEP_CPP_PPSTR) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\lex\special.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_SPECI=\
	".\..\..\lex\lex.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\token.h"\
	".\..\..\lex\special.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\xfloat.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\parser.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	
NODEP_CPP_SPECI=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\special.obj" : $(SOURCE) $(DEP_CPP_SPECI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_SPECI=\
	".\..\..\lex\lex.h"\
	".\..\..\lex\macro.h"\
	".\..\..\lex\token.h"\
	".\..\..\lex\special.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\xfloat.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\parser.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\special.obj" : $(SOURCE) $(DEP_CPP_SPECI) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\special.sbr" : $(SOURCE) $(DEP_CPP_SPECI) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\lex\macro.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_MACRO=\
	".\..\..\lex\lex.h"\
	".\..\..\lex\macro.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\xfloat.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\parser.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	
NODEP_CPP_MACRO=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\macro.obj" : $(SOURCE) $(DEP_CPP_MACRO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_MACRO=\
	".\..\..\lex\lex.h"\
	".\..\..\lex\macro.h"\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\xfloat.h"\
	".\..\..\comp\node.h"\
	".\..\..\comp\compile.h"\
	".\..\..\comp\parser.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\macro.obj" : $(SOURCE) $(DEP_CPP_MACRO) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\macro.sbr" : $(SOURCE) $(DEP_CPP_MACRO) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\lpc\lpc.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_LPC_C=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\comp\csupport.h"\
	".\..\..\lpc\list"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_LPC_C=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	".\..\..\lpc\driver.c"\
	".\..\..\lpc\auto.c"\
	".\..\..\lpc\objregd.c"\
	".\..\..\lpc\rsrcd.c"\
	".\..\..\lpc\accessd.c"\
	".\..\..\lpc\userd.c"\
	".\..\..\lpc\api_objreg.c"\
	".\..\..\lpc\api_rsrc.c"\
	".\..\..\lpc\api_access.c"\
	".\..\..\lpc\api_user.c"\
	".\..\..\lpc\lib_connection.c"\
	".\..\..\lpc\lib_user.c"\
	".\..\..\lpc\lib_wiztool.c"\
	".\..\..\lpc\rsrc.c"\
	".\..\..\lpc\telnet.c"\
	".\..\..\lpc\binary.c"\
	".\..\..\lpc\user.c"\
	".\..\..\lpc\wiztool.c"\
	

"$(INTDIR)\lpc.obj" : $(SOURCE) $(DEP_CPP_LPC_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_LPC_C=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\comp\csupport.h"\
	".\..\..\lpc\list"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_LPC_C=\
	".\..\..\lpc\driver.c"\
	".\..\..\lpc\auto.c"\
	".\..\..\lpc\objregd.c"\
	".\..\..\lpc\rsrcd.c"\
	".\..\..\lpc\accessd.c"\
	".\..\..\lpc\userd.c"\
	".\..\..\lpc\api_objreg.c"\
	".\..\..\lpc\api_rsrc.c"\
	".\..\..\lpc\api_access.c"\
	".\..\..\lpc\api_user.c"\
	".\..\..\lpc\lib_connection.c"\
	".\..\..\lpc\lib_user.c"\
	".\..\..\lpc\lib_wiztool.c"\
	".\..\..\lpc\rsrc.c"\
	".\..\..\lpc\telnet.c"\
	".\..\..\lpc\binary.c"\
	".\..\..\lpc\user.c"\
	".\..\..\lpc\wiztool.c"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\lpc.obj" : $(SOURCE) $(DEP_CPP_LPC_C) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\lpc.sbr" : $(SOURCE) $(DEP_CPP_LPC_C) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\time.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_TIME_=\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_TIME_=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\time.obj" : $(SOURCE) $(DEP_CPP_TIME_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_TIME_=\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

"$(INTDIR)\time.obj" : $(SOURCE) $(DEP_CPP_TIME_) "$(INTDIR)"

"$(INTDIR)\time.sbr" : $(SOURCE) $(DEP_CPP_TIME_) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\dosfile.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_DOSFI=\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_DOSFI=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\dosfile.obj" : $(SOURCE) $(DEP_CPP_DOSFI) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_DOSFI=\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

"$(INTDIR)\dosfile.obj" : $(SOURCE) $(DEP_CPP_DOSFI) "$(INTDIR)"

"$(INTDIR)\dosfile.sbr" : $(SOURCE) $(DEP_CPP_DOSFI) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\local.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_LOCAL=\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_LOCAL=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\local.obj" : $(SOURCE) $(DEP_CPP_LOCAL) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_LOCAL=\
	".\..\..\dgd.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

"$(INTDIR)\local.obj" : $(SOURCE) $(DEP_CPP_LOCAL) "$(INTDIR)"

"$(INTDIR)\local.sbr" : $(SOURCE) $(DEP_CPP_LOCAL) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\connect.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_CONNE=\
	".\..\..\dgd.h"\
	".\..\..\comm.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_CONNE=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\connect.obj" : $(SOURCE) $(DEP_CPP_CONNE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_CONNE=\
	".\..\..\dgd.h"\
	".\..\..\comm.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

"$(INTDIR)\connect.obj" : $(SOURCE) $(DEP_CPP_CONNE) "$(INTDIR)"

"$(INTDIR)\connect.sbr" : $(SOURCE) $(DEP_CPP_CONNE) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\MainFrame.cpp
DEP_CPP_MAINF=\
	".\StdAfx.h"\
	".\windgd.h"\
	".\MainFrame.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\MainFrame.obj" : $(SOURCE) $(DEP_CPP_MAINF) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


"$(INTDIR)\MainFrame.obj" : $(SOURCE) $(DEP_CPP_MAINF) "$(INTDIR)"

"$(INTDIR)\MainFrame.sbr" : $(SOURCE) $(DEP_CPP_MAINF) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\parser\srp.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_SRP_C=\
	".\..\..\dgd.h"\
	".\..\..\parser\srp.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_SRP_C=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\srp.obj" : $(SOURCE) $(DEP_CPP_SRP_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_SRP_C=\
	".\..\..\dgd.h"\
	".\..\..\parser\srp.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\srp.obj" : $(SOURCE) $(DEP_CPP_SRP_C) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\srp.sbr" : $(SOURCE) $(DEP_CPP_SRP_C) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\parser\parse.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_PARSE_=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\parser\grammar.h"\
	".\..\..\parser\dfa.h"\
	".\..\..\parser\srp.h"\
	".\..\..\parser\parse.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_PARSE_=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\parse.obj" : $(SOURCE) $(DEP_CPP_PARSE_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_PARSE_=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\parser\grammar.h"\
	".\..\..\parser\dfa.h"\
	".\..\..\parser\srp.h"\
	".\..\..\parser\parse.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\parse.obj" : $(SOURCE) $(DEP_CPP_PARSE_) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\parse.sbr" : $(SOURCE) $(DEP_CPP_PARSE_) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\parser\grammar.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_GRAMM=\
	".\..\..\dgd.h"\
	".\..\..\hash.h"\
	".\..\..\str.h"\
	".\..\..\parser\grammar.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_GRAMM=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\grammar.obj" : $(SOURCE) $(DEP_CPP_GRAMM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_GRAMM=\
	".\..\..\dgd.h"\
	".\..\..\hash.h"\
	".\..\..\str.h"\
	".\..\..\parser\grammar.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\grammar.obj" : $(SOURCE) $(DEP_CPP_GRAMM) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\grammar.sbr" : $(SOURCE) $(DEP_CPP_GRAMM) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\parser\dfa.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_DFA_C=\
	".\..\..\dgd.h"\
	".\..\..\hash.h"\
	".\..\..\str.h"\
	".\..\..\parser\dfa.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	
NODEP_CPP_DFA_C=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\dfa.obj" : $(SOURCE) $(DEP_CPP_DFA_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_DFA_C=\
	".\..\..\dgd.h"\
	".\..\..\hash.h"\
	".\..\..\str.h"\
	".\..\..\parser\dfa.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\dfa.obj" : $(SOURCE) $(DEP_CPP_DFA_C) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\dfa.sbr" : $(SOURCE) $(DEP_CPP_DFA_C) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\project\dgd\src\sdata.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_SDATA=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\call_out.h"\
	".\..\..\parser\parse.h"\
	".\..\..\comp\csupport.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	{$(INCLUDE)}"\SYS\TYPES.H"\
	{$(INCLUDE)}"\SYS\STAT.H"\
	".\..\telnet.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	
NODEP_CPP_SDATA=\
	".\..\..\macdgd.h"\
	".\..\..\telnet.h"\
	

"$(INTDIR)\sdata.obj" : $(SOURCE) $(DEP_CPP_SDATA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_SDATA=\
	".\..\..\dgd.h"\
	".\..\..\str.h"\
	".\..\..\array.h"\
	".\..\..\object.h"\
	".\..\..\interpret.h"\
	".\..\..\data.h"\
	".\..\..\call_out.h"\
	".\..\..\parser\parse.h"\
	".\..\..\comp\csupport.h"\
	".\..\..\host.h"\
	".\..\..\config.h"\
	".\..\..\alloc.h"\
	".\..\..\hash.h"\
	".\..\..\swap.h"\
	

BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\sdata.obj" : $(SOURCE) $(DEP_CPP_SDATA) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\sdata.sbr" : $(SOURCE) $(DEP_CPP_SDATA) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
