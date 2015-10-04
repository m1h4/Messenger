#pragma once

#include "MessengerView.h"

class CMessengerFrame : public CFrameWnd
{
	DECLARE_DYNAMIC(CMessengerFrame)

public:
	CMessengerFrame();
	virtual ~CMessengerFrame();

// Attributes
public:

// Operations
public:

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

// Implementation
public:
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

public:  // control bar embedded members
	CStatusBar			m_wndStatusBar;
	CMessengerView		m_wndView;

// Message Map
protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSetFocus(CWnd *pOldWnd);

	DECLARE_MESSAGE_MAP()

	afx_msg void OnDestroy();
public:
	virtual void RecalcLayout(BOOL bNotify = TRUE);
	BOOL CreateTray(void);
	BOOL DestroyTray(void);
};


