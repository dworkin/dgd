class CMainFrame : public CFrameWnd {
    DECLARE_DYNCREATE(CMainFrame)

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	//}}AFX_VIRTUAL

// Implementation
	// Generated message map functions
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();
	//}}AFX_MSG
    DECLARE_MESSAGE_MAP()

public:
    void addmessage(char *mesg);

private:
    void AddMessage();
    char *Message;		/* current message */

    CListBox *listbox;		/* message list */
    int lsize;			/* list size */
    BOOL lldone;		/* last line done */
};
