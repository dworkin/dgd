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
