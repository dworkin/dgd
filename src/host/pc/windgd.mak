# Microsoft Developer Studio Generated NMAKE File, Format Version 4.00
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
	-@erase ".\Release\ppstr.obj"
	-@erase ".\Release\MainFrame.obj"
	-@erase ".\Release\special.obj"
	-@erase ".\Release\builtin.obj"
	-@erase ".\Release\math.obj"
	-@erase ".\Release\lpc.obj"
	-@erase ".\Release\codegeni.obj"
	-@erase ".\Release\table.obj"
	-@erase ".\Release\extra.obj"
	-@erase ".\Release\hash.obj"
	-@erase ".\Release\edcmd.obj"
	-@erase ".\Release\data.obj"
	-@erase ".\Release\object.obj"
	-@erase ".\Release\node.obj"
	-@erase ".\Release\vars.obj"
	-@erase ".\Release\fileio.obj"
	-@erase ".\Release\interpret.obj"
	-@erase ".\Release\swap.obj"
	-@erase ".\Release\comm.obj"
	-@erase ".\Release\line.obj"
	-@erase ".\Release\control.obj"
	-@erase ".\Release\macro.obj"
	-@erase ".\Release\error.obj"
	-@erase ".\Release\str.obj"
	-@erase ".\Release\ppcontrol.obj"
	-@erase ".\Release\config.obj"
	-@erase ".\Release\dgd.obj"
	-@erase ".\Release\csupport.obj"
	-@erase ".\Release\std.obj"
	-@erase ".\Release\time.obj"
	-@erase ".\Release\file.obj"
	-@erase ".\Release\compile.obj"
	-@erase ".\Release\optimize.obj"
	-@erase ".\Release\path.obj"
	-@erase ".\Release\ed.obj"
	-@erase ".\Release\dosfile.obj"
	-@erase ".\Release\regexp.obj"
	-@erase ".\Release\windgd.obj"
	-@erase ".\Release\debug.obj"
	-@erase ".\Release\call_out.obj"
	-@erase ".\Release\local.obj"
	-@erase ".\Release\buffer.obj"
	-@erase ".\Release\parser.obj"
	-@erase ".\Release\cmdsub.obj"
	-@erase ".\Release\array.obj"
	-@erase ".\Release\crypt.obj"
	-@erase ".\Release\connect.obj"
	-@erase ".\Release\token.obj"
	-@erase ".\Release\simfloat.obj"
	-@erase ".\Release\alloc.obj"
	-@erase ".\Release\windgd.res"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /Zp4 /MT /W3 /GX /Ox /Ot /Og /Oi /Ob2 /Gf /Gy /I "\proj\dgd\src" /I "\proj\dgd\src\comp" /I "\proj\dgd\src\lex" /I "\proj\dgd\src\ed" /I "\proj\dgd\src\kfun" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /c
# SUBTRACT CPP /Ow /YX /Yc /Yu
CPP_PROJ=/nologo /Zp4 /MT /W3 /GX /Ox /Ot /Og /Oi /Ob2 /Gf /Gy /I\
 "\proj\dgd\src" /I "\proj\dgd\src\comp" /I "\proj\dgd\src\lex" /I\
 "\proj\dgd\src\ed" /I "\proj\dgd\src\kfun" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /D "_MBCS" /Fo"$(INTDIR)/" /c 
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
	"$(INTDIR)/ppstr.obj" \
	"$(INTDIR)/MainFrame.obj" \
	"$(INTDIR)/special.obj" \
	"$(INTDIR)/builtin.obj" \
	"$(INTDIR)/math.obj" \
	"$(INTDIR)/lpc.obj" \
	"$(INTDIR)/codegeni.obj" \
	"$(INTDIR)/table.obj" \
	"$(INTDIR)/extra.obj" \
	"$(INTDIR)/hash.obj" \
	"$(INTDIR)/edcmd.obj" \
	"$(INTDIR)/data.obj" \
	"$(INTDIR)/object.obj" \
	"$(INTDIR)/node.obj" \
	"$(INTDIR)/vars.obj" \
	"$(INTDIR)/fileio.obj" \
	"$(INTDIR)/interpret.obj" \
	"$(INTDIR)/swap.obj" \
	"$(INTDIR)/comm.obj" \
	"$(INTDIR)/line.obj" \
	"$(INTDIR)/control.obj" \
	"$(INTDIR)/macro.obj" \
	"$(INTDIR)/error.obj" \
	"$(INTDIR)/str.obj" \
	"$(INTDIR)/ppcontrol.obj" \
	"$(INTDIR)/config.obj" \
	"$(INTDIR)/dgd.obj" \
	"$(INTDIR)/csupport.obj" \
	"$(INTDIR)/std.obj" \
	"$(INTDIR)/time.obj" \
	"$(INTDIR)/file.obj" \
	"$(INTDIR)/compile.obj" \
	"$(INTDIR)/optimize.obj" \
	"$(INTDIR)/path.obj" \
	"$(INTDIR)/ed.obj" \
	"$(INTDIR)/dosfile.obj" \
	"$(INTDIR)/regexp.obj" \
	"$(INTDIR)/windgd.obj" \
	"$(INTDIR)/debug.obj" \
	"$(INTDIR)/call_out.obj" \
	"$(INTDIR)/local.obj" \
	"$(INTDIR)/buffer.obj" \
	"$(INTDIR)/parser.obj" \
	"$(INTDIR)/cmdsub.obj" \
	"$(INTDIR)/array.obj" \
	"$(INTDIR)/crypt.obj" \
	"$(INTDIR)/connect.obj" \
	"$(INTDIR)/token.obj" \
	"$(INTDIR)/simfloat.obj" \
	"$(INTDIR)/alloc.obj" \
	"$(INTDIR)/windgd.res"

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
	-@erase ".\Debug\edcmd.sbr"
	-@erase ".\Debug\simfloat.sbr"
	-@erase ".\Debug\time.sbr"
	-@erase ".\Debug\windgd.sbr"
	-@erase ".\Debug\csupport.sbr"
	-@erase ".\Debug\file.sbr"
	-@erase ".\Debug\buffer.sbr"
	-@erase ".\Debug\interpret.sbr"
	-@erase ".\Debug\codegeni.sbr"
	-@erase ".\Debug\dosfile.sbr"
	-@erase ".\Debug\parser.sbr"
	-@erase ".\Debug\cmdsub.sbr"
	-@erase ".\Debug\vars.sbr"
	-@erase ".\Debug\array.sbr"
	-@erase ".\Debug\optimize.sbr"
	-@erase ".\Debug\crypt.sbr"
	-@erase ".\Debug\path.sbr"
	-@erase ".\Debug\macro.sbr"
	-@erase ".\Debug\MainFrame.sbr"
	-@erase ".\Debug\dgd.sbr"
	-@erase ".\Debug\special.sbr"
	-@erase ".\Debug\extra.sbr"
	-@erase ".\Debug\object.sbr"
	-@erase ".\Debug\fileio.sbr"
	-@erase ".\Debug\math.sbr"
	-@erase ".\Debug\regexp.sbr"
	-@erase ".\Debug\hash.sbr"
	-@erase ".\Debug\debug.sbr"
	-@erase ".\Debug\data.sbr"
	-@erase ".\Debug\config.sbr"
	-@erase ".\Debug\local.sbr"
	-@erase ".\Debug\node.sbr"
	-@erase ".\Debug\error.sbr"
	-@erase ".\Debug\control.sbr"
	-@erase ".\Debug\ppcontrol.sbr"
	-@erase ".\Debug\str.sbr"
	-@erase ".\Debug\token.sbr"
	-@erase ".\Debug\swap.sbr"
	-@erase ".\Debug\std.sbr"
	-@erase ".\Debug\comm.sbr"
	-@erase ".\Debug\call_out.sbr"
	-@erase ".\Debug\line.sbr"
	-@erase ".\Debug\connect.sbr"
	-@erase ".\Debug\alloc.sbr"
	-@erase ".\Debug\ppstr.sbr"
	-@erase ".\Debug\table.sbr"
	-@erase ".\Debug\ed.sbr"
	-@erase ".\Debug\builtin.sbr"
	-@erase ".\Debug\lpc.sbr"
	-@erase ".\Debug\compile.sbr"
	-@erase ".\Debug\dgd.exe"
	-@erase ".\Debug\token.obj"
	-@erase ".\Debug\swap.obj"
	-@erase ".\Debug\std.obj"
	-@erase ".\Debug\comm.obj"
	-@erase ".\Debug\call_out.obj"
	-@erase ".\Debug\line.obj"
	-@erase ".\Debug\connect.obj"
	-@erase ".\Debug\alloc.obj"
	-@erase ".\Debug\ppstr.obj"
	-@erase ".\Debug\table.obj"
	-@erase ".\Debug\ed.obj"
	-@erase ".\Debug\builtin.obj"
	-@erase ".\Debug\lpc.obj"
	-@erase ".\Debug\compile.obj"
	-@erase ".\Debug\edcmd.obj"
	-@erase ".\Debug\simfloat.obj"
	-@erase ".\Debug\time.obj"
	-@erase ".\Debug\windgd.obj"
	-@erase ".\Debug\csupport.obj"
	-@erase ".\Debug\file.obj"
	-@erase ".\Debug\buffer.obj"
	-@erase ".\Debug\interpret.obj"
	-@erase ".\Debug\codegeni.obj"
	-@erase ".\Debug\dosfile.obj"
	-@erase ".\Debug\parser.obj"
	-@erase ".\Debug\cmdsub.obj"
	-@erase ".\Debug\vars.obj"
	-@erase ".\Debug\array.obj"
	-@erase ".\Debug\optimize.obj"
	-@erase ".\Debug\crypt.obj"
	-@erase ".\Debug\path.obj"
	-@erase ".\Debug\macro.obj"
	-@erase ".\Debug\MainFrame.obj"
	-@erase ".\Debug\dgd.obj"
	-@erase ".\Debug\special.obj"
	-@erase ".\Debug\extra.obj"
	-@erase ".\Debug\object.obj"
	-@erase ".\Debug\fileio.obj"
	-@erase ".\Debug\math.obj"
	-@erase ".\Debug\regexp.obj"
	-@erase ".\Debug\hash.obj"
	-@erase ".\Debug\debug.obj"
	-@erase ".\Debug\data.obj"
	-@erase ".\Debug\config.obj"
	-@erase ".\Debug\local.obj"
	-@erase ".\Debug\node.obj"
	-@erase ".\Debug\error.obj"
	-@erase ".\Debug\control.obj"
	-@erase ".\Debug\ppcontrol.obj"
	-@erase ".\Debug\str.obj"
	-@erase ".\Debug\windgd.res"
	-@erase ".\Debug\dgd.ilk"
	-@erase ".\Debug\dgd.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /c
# ADD CPP /nologo /Zp4 /MTd /W3 /Gm /GX /Zi /Od /I "\proj\dgd\src" /I "\proj\dgd\src\comp" /I "\proj\dgd\src\lex" /I "\proj\dgd\src\ed" /I "\proj\dgd\src\kfun" /D "WIN32" /D "DEBUG" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /FR /c
# SUBTRACT CPP /YX /Yc /Yu
CPP_PROJ=/nologo /Zp4 /MTd /W3 /Gm /GX /Zi /Od /I "\proj\dgd\src" /I\
 "\proj\dgd\src\comp" /I "\proj\dgd\src\lex" /I "\proj\dgd\src\ed" /I\
 "\proj\dgd\src\kfun" /D "WIN32" /D "DEBUG" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS"\
 /FR"$(INTDIR)/" /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
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
	"$(INTDIR)/edcmd.sbr" \
	"$(INTDIR)/simfloat.sbr" \
	"$(INTDIR)/time.sbr" \
	"$(INTDIR)/windgd.sbr" \
	"$(INTDIR)/csupport.sbr" \
	"$(INTDIR)/file.sbr" \
	"$(INTDIR)/buffer.sbr" \
	"$(INTDIR)/interpret.sbr" \
	"$(INTDIR)/codegeni.sbr" \
	"$(INTDIR)/dosfile.sbr" \
	"$(INTDIR)/parser.sbr" \
	"$(INTDIR)/cmdsub.sbr" \
	"$(INTDIR)/vars.sbr" \
	"$(INTDIR)/array.sbr" \
	"$(INTDIR)/optimize.sbr" \
	"$(INTDIR)/crypt.sbr" \
	"$(INTDIR)/path.sbr" \
	"$(INTDIR)/macro.sbr" \
	"$(INTDIR)/MainFrame.sbr" \
	"$(INTDIR)/dgd.sbr" \
	"$(INTDIR)/special.sbr" \
	"$(INTDIR)/extra.sbr" \
	"$(INTDIR)/object.sbr" \
	"$(INTDIR)/fileio.sbr" \
	"$(INTDIR)/math.sbr" \
	"$(INTDIR)/regexp.sbr" \
	"$(INTDIR)/hash.sbr" \
	"$(INTDIR)/debug.sbr" \
	"$(INTDIR)/data.sbr" \
	"$(INTDIR)/config.sbr" \
	"$(INTDIR)/local.sbr" \
	"$(INTDIR)/node.sbr" \
	"$(INTDIR)/error.sbr" \
	"$(INTDIR)/control.sbr" \
	"$(INTDIR)/ppcontrol.sbr" \
	"$(INTDIR)/str.sbr" \
	"$(INTDIR)/token.sbr" \
	"$(INTDIR)/swap.sbr" \
	"$(INTDIR)/std.sbr" \
	"$(INTDIR)/comm.sbr" \
	"$(INTDIR)/call_out.sbr" \
	"$(INTDIR)/line.sbr" \
	"$(INTDIR)/connect.sbr" \
	"$(INTDIR)/alloc.sbr" \
	"$(INTDIR)/ppstr.sbr" \
	"$(INTDIR)/table.sbr" \
	"$(INTDIR)/ed.sbr" \
	"$(INTDIR)/builtin.sbr" \
	"$(INTDIR)/lpc.sbr" \
	"$(INTDIR)/compile.sbr"

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
	"$(INTDIR)/token.obj" \
	"$(INTDIR)/swap.obj" \
	"$(INTDIR)/std.obj" \
	"$(INTDIR)/comm.obj" \
	"$(INTDIR)/call_out.obj" \
	"$(INTDIR)/line.obj" \
	"$(INTDIR)/connect.obj" \
	"$(INTDIR)/alloc.obj" \
	"$(INTDIR)/ppstr.obj" \
	"$(INTDIR)/table.obj" \
	"$(INTDIR)/ed.obj" \
	"$(INTDIR)/builtin.obj" \
	"$(INTDIR)/lpc.obj" \
	"$(INTDIR)/compile.obj" \
	"$(INTDIR)/edcmd.obj" \
	"$(INTDIR)/simfloat.obj" \
	"$(INTDIR)/time.obj" \
	"$(INTDIR)/windgd.obj" \
	"$(INTDIR)/csupport.obj" \
	"$(INTDIR)/file.obj" \
	"$(INTDIR)/buffer.obj" \
	"$(INTDIR)/interpret.obj" \
	"$(INTDIR)/codegeni.obj" \
	"$(INTDIR)/dosfile.obj" \
	"$(INTDIR)/parser.obj" \
	"$(INTDIR)/cmdsub.obj" \
	"$(INTDIR)/vars.obj" \
	"$(INTDIR)/array.obj" \
	"$(INTDIR)/optimize.obj" \
	"$(INTDIR)/crypt.obj" \
	"$(INTDIR)/path.obj" \
	"$(INTDIR)/macro.obj" \
	"$(INTDIR)/MainFrame.obj" \
	"$(INTDIR)/dgd.obj" \
	"$(INTDIR)/special.obj" \
	"$(INTDIR)/extra.obj" \
	"$(INTDIR)/object.obj" \
	"$(INTDIR)/fileio.obj" \
	"$(INTDIR)/math.obj" \
	"$(INTDIR)/regexp.obj" \
	"$(INTDIR)/hash.obj" \
	"$(INTDIR)/debug.obj" \
	"$(INTDIR)/data.obj" \
	"$(INTDIR)/config.obj" \
	"$(INTDIR)/local.obj" \
	"$(INTDIR)/node.obj" \
	"$(INTDIR)/error.obj" \
	"$(INTDIR)/control.obj" \
	"$(INTDIR)/ppcontrol.obj" \
	"$(INTDIR)/str.obj" \
	"$(INTDIR)/windgd.res"

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
DEP_CPP_WINDG=\
	".\StdAfx.h"\
	".\MainFrame.h"\
	".\windgd.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\windgd.obj" : $(SOURCE) $(DEP_CPP_WINDG) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\swap.c
DEP_CPP_SWAP_=\
	"\proj\dgd\src\dgd.h"\
	".\..\..\swap.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\swap.obj" : $(SOURCE) $(DEP_CPP_SWAP_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\array.c
DEP_CPP_ARRAY=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\array.obj" : $(SOURCE) $(DEP_CPP_ARRAY) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\call_out.c
DEP_CPP_CALL_=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\call_out.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\call_out.obj" : $(SOURCE) $(DEP_CPP_CALL_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\comm.c
DEP_CPP_COMM_=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\comm.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\comm.obj" : $(SOURCE) $(DEP_CPP_COMM_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\config.c
DEP_CPP_CONFI=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\path.h"\
	"\proj\dgd\src\ed.h"\
	"\proj\dgd\src\call_out.h"\
	"\proj\dgd\src\comm.h"\
	".\..\..\version.h"\
	"\proj\dgd\src\lex\macro.h"\
	"\proj\dgd\src\lex\token.h"\
	"\proj\dgd\src\lex\ppcontrol.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\parser.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\comp\csupport.h"\
	"\proj\dgd\src\kfun\table.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\config.obj" : $(SOURCE) $(DEP_CPP_CONFI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\data.c
DEP_CPP_DATA_=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\interpret.h"\
	".\..\..\swap.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\comp\csupport.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\data.obj" : $(SOURCE) $(DEP_CPP_DATA_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\dgd.c
DEP_CPP_DGD_C=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\ed.h"\
	"\proj\dgd\src\call_out.h"\
	"\proj\dgd\src\comm.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\dgd.obj" : $(SOURCE) $(DEP_CPP_DGD_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\ed.c
DEP_CPP_ED_C12=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\ed\edcmd.h"\
	"\proj\dgd\src\ed.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	".\..\..\ed\regexp.h"\
	".\..\..\ed\buffer.h"\
	".\..\..\ed\vars.h"\
	".\..\..\ed\line.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\ed.obj" : $(SOURCE) $(DEP_CPP_ED_C12) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\ed.obj" : $(SOURCE) $(DEP_CPP_ED_C12) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\ed.sbr" : $(SOURCE) $(DEP_CPP_ED_C12) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Proj\dgd\src\error.c
DEP_CPP_ERROR=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\comm.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\error.obj" : $(SOURCE) $(DEP_CPP_ERROR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\hash.c
DEP_CPP_HASH_=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\hash.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\hash.obj" : $(SOURCE) $(DEP_CPP_HASH_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\interpret.c
DEP_CPP_INTER=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\comp\control.h"\
	"\proj\dgd\src\comp\csupport.h"\
	"\proj\dgd\src\kfun\table.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\interpret.obj" : $(SOURCE) $(DEP_CPP_INTER) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\object.c
DEP_CPP_OBJEC=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\object.obj" : $(SOURCE) $(DEP_CPP_OBJEC) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\path.c
DEP_CPP_PATH_=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\path.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\path.obj" : $(SOURCE) $(DEP_CPP_PATH_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\str.c
DEP_CPP_STR_C=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\hash.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\str.obj" : $(SOURCE) $(DEP_CPP_STR_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\alloc.c
DEP_CPP_ALLOC=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\alloc.obj" : $(SOURCE) $(DEP_CPP_ALLOC) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\comp\codegeni.c
DEP_CPP_CODEG=\
	".\..\..\comp\comp.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\kfun\table.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\control.h"\
	".\..\..\comp\codegen.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\codegeni.obj" : $(SOURCE) $(DEP_CPP_CODEG) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\comp\compile.c
DEP_CPP_COMPI=\
	".\..\..\comp\comp.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\path.h"\
	"\proj\dgd\src\lex\macro.h"\
	"\proj\dgd\src\lex\token.h"\
	"\proj\dgd\src\lex\ppcontrol.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\control.h"\
	".\..\..\comp\optimize.h"\
	".\..\..\comp\codegen.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\compile.obj" : $(SOURCE) $(DEP_CPP_COMPI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\comp\control.c
DEP_CPP_CONTR=\
	".\..\..\comp\comp.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\hash.h"\
	"\proj\dgd\src\kfun\table.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\comp\control.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\control.obj" : $(SOURCE) $(DEP_CPP_CONTR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\comp\csupport.c
DEP_CPP_CSUPP=\
	".\..\..\comp\comp.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\kfun\table.h"\
	"\proj\dgd\src\comp\control.h"\
	"\proj\dgd\src\comp\csupport.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\csupport.obj" : $(SOURCE) $(DEP_CPP_CSUPP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\comp\node.c
DEP_CPP_NODE_=\
	".\..\..\comp\comp.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\lex\macro.h"\
	"\proj\dgd\src\lex\token.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\node.obj" : $(SOURCE) $(DEP_CPP_NODE_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\comp\optimize.c
DEP_CPP_OPTIM=\
	".\..\..\comp\comp.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\compile.h"\
	".\..\..\comp\optimize.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\optimize.obj" : $(SOURCE) $(DEP_CPP_OPTIM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\comp\parser.c
DEP_CPP_PARSE=\
	".\..\..\comp\comp.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\lex\macro.h"\
	"\proj\dgd\src\lex\token.h"\
	"\proj\dgd\src\lex\ppcontrol.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\parser.obj" : $(SOURCE) $(DEP_CPP_PARSE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\ed\buffer.c
DEP_CPP_BUFFE=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\buffer.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	".\..\..\ed\line.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\buffer.obj" : $(SOURCE) $(DEP_CPP_BUFFE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\ed\cmdsub.c
DEP_CPP_CMDSU=\
	".\..\..\ed\ed.h"\
	"\proj\dgd\src\ed\edcmd.h"\
	".\..\..\ed\fileio.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	".\..\..\ed\regexp.h"\
	".\..\..\ed\buffer.h"\
	".\..\..\ed\vars.h"\
	".\..\..\ed\line.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\cmdsub.obj" : $(SOURCE) $(DEP_CPP_CMDSU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\ed\edcmd.c
DEP_CPP_EDCMD=\
	".\..\..\ed\ed.h"\
	"\proj\dgd\src\ed\edcmd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	".\..\..\ed\regexp.h"\
	".\..\..\ed\buffer.h"\
	".\..\..\ed\vars.h"\
	".\..\..\ed\line.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\edcmd.obj" : $(SOURCE) $(DEP_CPP_EDCMD) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\ed\fileio.c
DEP_CPP_FILEI=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\buffer.h"\
	"\proj\dgd\src\path.h"\
	".\..\..\ed\fileio.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	".\..\..\ed\line.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\fileio.obj" : $(SOURCE) $(DEP_CPP_FILEI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\ed\line.c
DEP_CPP_LINE_=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\line.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\line.obj" : $(SOURCE) $(DEP_CPP_LINE_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\ed\regexp.c
DEP_CPP_REGEX=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\regexp.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\regexp.obj" : $(SOURCE) $(DEP_CPP_REGEX) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\ed\vars.c
DEP_CPP_VARS_=\
	".\..\..\ed\ed.h"\
	".\..\..\ed\vars.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\vars.obj" : $(SOURCE) $(DEP_CPP_VARS_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\host\simfloat.c
DEP_CPP_SIMFL=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\simfloat.obj" : $(SOURCE) $(DEP_CPP_SIMFL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\host\crypt.c
DEP_CPP_CRYPT=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\crypt.obj" : $(SOURCE) $(DEP_CPP_CRYPT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\kfun\table.c
DEP_CPP_TABLE=\
	".\..\..\kfun\kfun.h"\
	"\proj\dgd\src\kfun\table.h"\
	".\..\..\kfun\builtin.c"\
	".\..\..\kfun\std.c"\
	".\..\..\kfun\file.c"\
	".\..\..\kfun\math.c"\
	".\..\..\kfun\extra.c"\
	".\..\..\kfun\debug.c"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	"\proj\dgd\src\path.h"\
	"\proj\dgd\src\comm.h"\
	"\proj\dgd\src\call_out.h"\
	"\proj\dgd\src\ed.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\control.h"\
	"\proj\dgd\src\comp\compile.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\table.obj" : $(SOURCE) $(DEP_CPP_TABLE) "$(INTDIR)"\
 ".\..\..\kfun\builtin.c" ".\..\..\kfun\std.c" ".\..\..\kfun\file.c"\
 ".\..\..\kfun\math.c" ".\..\..\kfun\extra.c" ".\..\..\kfun\debug.c"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


BuildCmds= \
	$(CPP) $(CPP_PROJ) $(SOURCE) \
	

"$(INTDIR)\table.obj" : $(SOURCE) $(DEP_CPP_TABLE) "$(INTDIR)"
   $(BuildCmds)

"$(INTDIR)\table.sbr" : $(SOURCE) $(DEP_CPP_TABLE) "$(INTDIR)"
   $(BuildCmds)

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\Proj\dgd\src\kfun\debug.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_DEBUG=\
	".\..\..\kfun\kfun.h"\
	"\proj\dgd\src\comp\control.h"\
	"\proj\dgd\src\kfun\table.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

"$(INTDIR)\debug.obj" : $(SOURCE) $(DEP_CPP_DEBUG) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_DEBUG=\
	".\..\..\kfun\kfun.h"\
	"\proj\dgd\src\comp\control.h"\
	"\proj\dgd\src\kfun\table.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

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

SOURCE=\Proj\dgd\src\kfun\extra.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_EXTRA=\
	".\..\..\kfun\kfun.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

"$(INTDIR)\extra.obj" : $(SOURCE) $(DEP_CPP_EXTRA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_EXTRA=\
	".\..\..\kfun\kfun.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

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

SOURCE=\Proj\dgd\src\kfun\file.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_FILE_=\
	".\..\..\kfun\kfun.h"\
	"\proj\dgd\src\path.h"\
	"\proj\dgd\src\ed.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

"$(INTDIR)\file.obj" : $(SOURCE) $(DEP_CPP_FILE_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_FILE_=\
	".\..\..\kfun\kfun.h"\
	"\proj\dgd\src\path.h"\
	"\proj\dgd\src\ed.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

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

SOURCE=\Proj\dgd\src\kfun\math.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_MATH_=\
	".\..\..\kfun\kfun.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

"$(INTDIR)\math.obj" : $(SOURCE) $(DEP_CPP_MATH_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_MATH_=\
	".\..\..\kfun\kfun.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

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

SOURCE=\Proj\dgd\src\kfun\std.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_STD_C=\
	".\..\..\kfun\kfun.h"\
	"\proj\dgd\src\path.h"\
	"\proj\dgd\src\comm.h"\
	"\proj\dgd\src\call_out.h"\
	"\proj\dgd\src\ed.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\control.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

"$(INTDIR)\std.obj" : $(SOURCE) $(DEP_CPP_STD_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_STD_C=\
	".\..\..\kfun\kfun.h"\
	"\proj\dgd\src\path.h"\
	"\proj\dgd\src\comm.h"\
	"\proj\dgd\src\call_out.h"\
	"\proj\dgd\src\ed.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\control.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

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

SOURCE=\Proj\dgd\src\kfun\builtin.c

!IF  "$(CFG)" == "windgd - Win32 Release"

DEP_CPP_BUILT=\
	".\..\..\kfun\kfun.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

"$(INTDIR)\builtin.obj" : $(SOURCE) $(DEP_CPP_BUILT) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"

DEP_CPP_BUILT=\
	".\..\..\kfun\kfun.h"\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

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

SOURCE=\Proj\dgd\src\lex\token.c
DEP_CPP_TOKEN=\
	".\..\..\lex\lex.h"\
	"\proj\dgd\src\path.h"\
	"\proj\dgd\src\lex\macro.h"\
	".\..\..\lex\special.h"\
	".\..\..\lex\ppstr.h"\
	"\proj\dgd\src\lex\token.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\comp\parser.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\token.obj" : $(SOURCE) $(DEP_CPP_TOKEN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\lex\ppcontrol.c
DEP_CPP_PPCON=\
	".\..\..\lex\lex.h"\
	"\proj\dgd\src\lex\macro.h"\
	".\..\..\lex\special.h"\
	".\..\..\lex\ppstr.h"\
	"\proj\dgd\src\lex\token.h"\
	"\proj\dgd\src\path.h"\
	"\proj\dgd\src\lex\ppcontrol.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\comp\parser.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\ppcontrol.obj" : $(SOURCE) $(DEP_CPP_PPCON) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\lex\ppstr.c
DEP_CPP_PPSTR=\
	".\..\..\lex\lex.h"\
	".\..\..\lex\ppstr.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\comp\parser.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\ppstr.obj" : $(SOURCE) $(DEP_CPP_PPSTR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\lex\special.c
DEP_CPP_SPECI=\
	".\..\..\lex\lex.h"\
	"\proj\dgd\src\lex\macro.h"\
	"\proj\dgd\src\lex\token.h"\
	".\..\..\lex\special.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\comp\parser.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\special.obj" : $(SOURCE) $(DEP_CPP_SPECI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\lex\macro.c
DEP_CPP_MACRO=\
	".\..\..\lex\lex.h"\
	"\proj\dgd\src\lex\macro.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\xfloat.h"\
	"\proj\dgd\src\comp\node.h"\
	"\proj\dgd\src\comp\compile.h"\
	"\proj\dgd\src\comp\parser.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\macro.obj" : $(SOURCE) $(DEP_CPP_MACRO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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

SOURCE=\Proj\dgd\src\lpc\lpc.c
DEP_CPP_LPC_C=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\interpret.h"\
	"\proj\dgd\src\data.h"\
	"\proj\dgd\src\comp\csupport.h"\
	".\..\..\lpc\list"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\lpc.obj" : $(SOURCE) $(DEP_CPP_LPC_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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
DEP_CPP_TIME_=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\time.obj" : $(SOURCE) $(DEP_CPP_TIME_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


"$(INTDIR)\time.obj" : $(SOURCE) $(DEP_CPP_TIME_) "$(INTDIR)"

"$(INTDIR)\time.sbr" : $(SOURCE) $(DEP_CPP_TIME_) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\dosfile.c
DEP_CPP_DOSFI=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\dosfile.obj" : $(SOURCE) $(DEP_CPP_DOSFI) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


"$(INTDIR)\dosfile.obj" : $(SOURCE) $(DEP_CPP_DOSFI) "$(INTDIR)"

"$(INTDIR)\dosfile.sbr" : $(SOURCE) $(DEP_CPP_DOSFI) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\local.c
DEP_CPP_LOCAL=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\local.obj" : $(SOURCE) $(DEP_CPP_LOCAL) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


"$(INTDIR)\local.obj" : $(SOURCE) $(DEP_CPP_LOCAL) "$(INTDIR)"

"$(INTDIR)\local.sbr" : $(SOURCE) $(DEP_CPP_LOCAL) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\connect.c
DEP_CPP_CONNE=\
	"\proj\dgd\src\dgd.h"\
	"\proj\dgd\src\str.h"\
	"\proj\dgd\src\array.h"\
	"\proj\dgd\src\object.h"\
	"\proj\dgd\src\comm.h"\
	"\proj\dgd\src\config.h"\
	"\proj\dgd\src\alloc.h"\
	".\..\..\host.h"\
	{$(INCLUDE)}"\sys\Types.h"\
	{$(INCLUDE)}"\sys\Stat.h"\
	".\..\telnet.h"\
	"\proj\dgd\src\hash.h"\
	".\..\..\swap.h"\
	

!IF  "$(CFG)" == "windgd - Win32 Release"


"$(INTDIR)\connect.obj" : $(SOURCE) $(DEP_CPP_CONNE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "windgd - Win32 Debug"


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
# End Target
# End Project
################################################################################
