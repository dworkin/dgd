; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CMainFrame
LastTemplate=CFrameWnd
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "windgd.h"
LastPage=0

ClassCount=3
Class1=CWindgdApp
Class2=CAboutDlg

ResourceCount=2
Resource1=IDD_ABOUTBOX
Class3=CMainFrame
Resource2=IDR_MAINFRAME

[CLS:CWindgdApp]
Type=0
HeaderFile=windgd.h
ImplementationFile=windgd.cpp
Filter=N
BaseClass=CWinApp
VirtualFilter=AC
LastObject=CWindgdApp

[CLS:CAboutDlg]
Type=0
HeaderFile=windgd.cpp
ImplementationFile=windgd.cpp
Filter=D
LastObject=CAboutDlg
BaseClass=CDialog
VirtualFilter=dWC

[DLG:IDD_ABOUTBOX]
Type=1
Class=CAboutDlg
ControlCount=11
Control1=IDC_STATIC,static,1342177283
Control2=IDC_STATIC,static,1342308480
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342373889
Control5=IDC_STATIC,static,1342308352
Control6=IDC_STATIC,static,1342308352
Control7=IDC_STATIC,static,1342308352
Control8=IDC_STATIC,static,1342308352
Control9=IDC_STATIC,static,1342308352
Control10=IDC_STATIC,static,1342308352
Control11=IDC_STATIC,static,1342308352

[MNU:IDR_MAINFRAME]
Type=1
Class=?
Command1=ID_DGD_CONFIG
Command2=ID_DGD_RESTORE
Command3=ID_DGD_START
Command4=ID_APP_EXIT
Command5=ID_EDIT_UNDO
Command6=ID_EDIT_CUT
Command7=ID_EDIT_COPY
Command8=ID_EDIT_PASTE
Command9=ID_APP_ABOUT
CommandCount=9

[ACL:IDR_MAINFRAME]
Type=1
Class=CMainFrame
Command1=ID_DGD_CONFIG
Command2=ID_DGD_RESTORE
Command3=ID_DGD_START
Command4=ID_EDIT_UNDO
Command5=ID_EDIT_CUT
Command6=ID_EDIT_COPY
Command7=ID_EDIT_PASTE
Command8=ID_EDIT_UNDO
Command9=ID_EDIT_CUT
Command10=ID_EDIT_COPY
Command11=ID_EDIT_PASTE
Command12=ID_NEXT_PANE
Command13=ID_PREV_PANE
CommandCount=13

[CLS:CMainFrame]
Type=0
HeaderFile=MainFrame.h
ImplementationFile=MainFrame.cpp
BaseClass=CFrameWnd
Filter=T
LastObject=CMainFrame
VirtualFilter=fWC

