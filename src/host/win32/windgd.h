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

# ifndef __AFXWIN_H__
# error include 'stdafx.h' before including this file for PCH
# endif

# include "resource.h"


class CWindgdApp : public CWinApp {
// Overrides
	//{{AFX_VIRTUAL(CWindgdApp)
	public:
	virtual BOOL InitInstance();
	virtual BOOL SaveAllModified();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CWindgdApp)
	afx_msg void OnAppAbout();
	afx_msg void OnDgdConfig();
	afx_msg void OnUpdateDgdConfig(CCmdUI* pCmdUI);
	afx_msg void OnDgdRestore();
	afx_msg void OnUpdateDgdRestore(CCmdUI* pCmdUI);
	afx_msg void OnDgdStart();
	afx_msg void OnUpdateDgdStart(CCmdUI* pCmdUI);
	afx_msg void OnAppExit();
	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};
