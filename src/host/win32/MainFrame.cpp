/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateEditSelectAll)
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
    listbox->Create(LBS_EXTENDEDSEL | LBS_NOINTEGRALHEIGHT | LBS_USETABSTOPS | WS_VSCROLL,
		    rect, this, 0);
    listbox->InitStorage(MAXITEMS, MAXITEMS * 100);
    listbox->SetFont(CFont::FromHandle((HFONT) GetStockObject(SYSTEM_FIXED_FONT)));
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

void CMainFrame::OnEditCopy()
{
    int n, i, items[MAXITEMS];
    CString str, tmp;
    HGLOBAL data;
    LPVOID mem;

    if (!OpenClipboard() || !EmptyClipboard()) {
	return;
    }

    n = listbox->GetSelItems(MAXITEMS, items);
    str = "";
    for (i = 0; i < n; i++) {
	listbox->GetText(items[i], tmp);
	str += tmp + "\r\n";
    }

    data = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, str.GetLength() + 1);
    mem = GlobalLock(data);
    strcpy((char *) mem, str);
    GlobalUnlock(data);

    if (SetClipboardData(CF_TEXT, data) == (HANDLE) NULL) {
	GlobalFree(data);
    }
    CloseClipboard();
}

void CMainFrame::OnEditSelectAll()
{
    listbox->SetSel(-1);
}

void CMainFrame::OnUpdateEditCopy(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(listbox->GetSelCount() != 0);
}

void CMainFrame::OnUpdateEditSelectAll(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(lsize != 0);
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

	while (lsize == MAXITEMS || listbox->AddString(mesg) == LB_ERRSPACE) {
	    listbox->DeleteString(0);
	    --lsize;
	}
	lsize++;

	mesg = p;
    }

    listbox->SetTopIndex(lsize - 1);
}

void CMainFrame::addmessage(char *mesg)
{
    Message = mesg;
    SendMessage(WM_COMMAND, ID_DGD_MESG);
}
