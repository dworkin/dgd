# include "stdafx.h"
# include "windgd.h"
# include "MainFrame.h"

# ifdef _DEBUG
# define new DEBUG_NEW
# undef THIS_FILE
static char THIS_FILE[] = __FILE__;
# endif

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
    ON_COMMAND(ID_DGD_MESG, AddMessage)
END_MESSAGE_MAP()


int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    RECT rect;

    if (CFrameWnd::OnCreate(lpCreateStruct) == -1) {
	return -1;
    }

    GetClientRect(&rect);
    listbox = new CListBox();
    listbox->Create(LBS_NOINTEGRALHEIGHT | LBS_USETABSTOPS | WS_HSCROLL |
		    WS_VSCROLL,
		    rect, this, 0);
    listbox->SetFont(CFont::FromHandle((HFONT) GetStockObject(ANSI_FIXED_FONT)));
    listbox->ShowWindow(SW_SHOW);
    lsize = 0;
    lldone = TRUE;

    return 0;
}

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
    RECT rect;

    CFrameWnd::OnSize(nType, cx, cy);
    if (cx != 0 && cy != 0) {
	GetClientRect(&rect);
	listbox->MoveWindow(&rect);
    }
}

void CMainFrame::OnDestroy()
{
    CFrameWnd::OnDestroy();
    delete listbox;
}

void CMainFrame::AddMessage()
{
    char *mesg, *p;
    CString tmp;

    CString copy = Message;
    mesg = (char *) (LPCTSTR) copy;
    while (mesg != NULL && mesg[0] != '\0') {
	p = strchr(mesg, '\n');
	if (p != NULL) {
	    *p++ = '\0';
	}

	if (!lldone) {
	    listbox->GetText(--lsize, tmp);
	    listbox->DeleteString(lsize);
	    tmp += mesg;
	    mesg = (char *) (LPCTSTR) tmp;
	}
	lldone = (p != NULL);

	while (listbox->AddString(mesg) == LB_ERRSPACE) {
	    listbox->DeleteString(0);
	    --lsize;
	}
	lsize++;

	mesg = p;
    }

    listbox->SetCurSel(-1);
    listbox->SetTopIndex(lsize - 1);
}

void CMainFrame::addmessage(char *mesg)
{
    Message = mesg;
    SendMessage(WM_COMMAND, ID_DGD_MESG);
}
