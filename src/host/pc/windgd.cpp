# include "stdafx.h"
# include "MainFrame.h"
# include "windgd.h"
# include <process.h>

# ifdef _DEBUG
# define new DEBUG_NEW
# undef THIS_FILE
static char THIS_FILE[] = __FILE__;
# endif

static CWindgdApp	theApp;
static CMainFrame      *frame;
static int		dgd_started;
static CString		dgd_config;
static CString		dgd_restore;

extern "C" {

# include "dgd.h"
# undef exit

/*
 * NAME:	P->message()
 * DESCRIPTION:	output a message
 */
void P_message(char *mesg)
{
    frame->addmessage(mesg);
}

/*
 * NAME:	dgd_exit()
 * DESCRIPTION:	exit the DGD thread
 */
void dgd_exit(int code)
{
    exit(code);
}

}

/*
 * NAME:	run_dgd()
 * DESCRIPTION:	start the DGD thread
 */
static void run_dgd(void *dummy)
{
    int argc;
    char *argv[4];

    P_srandom(P_time());

    argv[0] = "dgd";
    argv[1] = (char *) (LPCTSTR) dgd_config;
    if (dgd_restore.IsEmpty()) {
	argc = 2;
	argv[2] = (char *) NULL;
    } else {
	argc = 3;
	argv[2] = (char *) (LPCTSTR) dgd_restore;
	argv[3] = (char *) NULL;
    }

    dgd_exit(dgd_main(argc, argv));
}


BEGIN_MESSAGE_MAP(CWindgdApp, CWinApp)
	//{{AFX_MSG_MAP(CWindgdApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_DGD_CONFIG, OnDgdConfig)
	ON_UPDATE_COMMAND_UI(ID_DGD_CONFIG, OnUpdateDgdConfig)
	ON_COMMAND(ID_DGD_RESTORE, OnDgdRestore)
	ON_UPDATE_COMMAND_UI(ID_DGD_RESTORE, OnUpdateDgdRestore)
	ON_COMMAND(ID_DGD_START, OnDgdStart)
	ON_UPDATE_COMMAND_UI(ID_DGD_START, OnUpdateDgdStart)
	//}}AFX_MSG_MAP
    // Standard file based document commands
END_MESSAGE_MAP()

BOOL CWindgdApp::InitInstance()
{
    char *cmdline, *p;

# ifdef _AFXDLL
    Enable3dControls();
# else
    Enable3dControlsStatic();
# endif

    /* All this is just for the registry */
    CSingleDocTemplate* pDocTemplate = new
	CSingleDocTemplate(IDR_MAINFRAME, NULL, NULL, NULL);
    AddDocTemplate(pDocTemplate);
    EnableShellOpen();
    RegisterShellFileTypes(TRUE);

    /* Create frame window */
    frame = new CMainFrame;
    if (!frame->LoadFrame(IDR_MAINFRAME)) {
	return FALSE;
    }
    m_pMainWnd = frame;
    frame->ShowWindow(m_nCmdShow);
    frame->UpdateWindow();

    /* parse command line */
    CString copy = m_lpCmdLine;
    cmdline = (char *) (LPCTSTR) copy;
    if (cmdline[0] != '\0' && cmdline[0] != '-' && cmdline[0] != '/') {
	p = strchr(cmdline, ' ');
	if (p != NULL) {
	    *p++ = '\0';
	    if (*p != '\0') {
		dgd_restore = p;
		p = strchr(cmdline, ' ');
		if (p != NULL) {
		    *p = '\0';
		}
	    }
	}
	p = cmdline + strlen(cmdline);
	if (cmdline[0] == '"' && p[-1] == '"') {
	    /* remove quotes around argument */
	    cmdline++;
	    p[-1] = '\0';
	}
	dgd_config = cmdline;
	OnDgdStart();
    }

    return TRUE;
}


void CWindgdApp::OnDgdConfig()
{
    CFileDialog config(TRUE, NULL, "config.dgd",
    		       OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		       "Config Files (*.dgd)|*.dgd|All Files (*.*)|*.*||");

    config.m_ofn.lpstrTitle = "Config File";
    if (config.DoModal() == IDOK) {
	dgd_config = config.GetPathName();
    }
}

void CWindgdApp::OnDgdRestore()
{
    CFileDialog restore(TRUE, NULL, NULL,
    			OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
    			"All Files (*.*)|*.*||");

    restore.m_ofn.lpstrTitle = "Restore File";
    if (restore.DoModal() == IDOK) {
	dgd_restore = restore.GetPathName();
    }
}

void CWindgdApp::OnDgdStart()
{
    dgd_started = TRUE;
    m_pMainWnd->SetWindowText("DGD - running");
    _beginthread(run_dgd, 0, NULL);
}

void CWindgdApp::OnUpdateDgdConfig(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!dgd_started);
}

void CWindgdApp::OnUpdateDgdStart(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!dgd_config.IsEmpty() && !dgd_started);
}

void CWindgdApp::OnUpdateDgdRestore(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!dgd_started);
}

BOOL CWindgdApp::SaveAllModified()
{
    return (!dgd_started ||
	    AfxMessageBox(
		"Are you sure you want to\nterminate the running process?",
		MB_ICONEXCLAMATION | MB_YESNO | MB_DEFBUTTON2) == IDYES);
}


class CAboutDlg : public CDialog {
public:
    CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CWindgdApp::OnAppAbout()
{
    CAboutDlg aboutDlg;
    aboutDlg.DoModal();
}
