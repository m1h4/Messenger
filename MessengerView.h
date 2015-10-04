#pragma once

#include "MessengerWnd.h"

// CMessengerView window

class CMessengerView : public CMessengerWnd
{
	DECLARE_DYNCREATE(CMessengerView);

// Construction
public:
	CMessengerView();
	virtual ~CMessengerView();

// Attributes
public:
	CStatic				m_wndStaticUser;
	CStatic				m_wndStaticPass;
	CStatic				m_wndStaticStatus;
	CComboBox			m_wndComboUser;
	CEdit				m_wndEditPass;
	CComboBox			m_wndComboStatus;
	CButton				m_wndSignIn;
	CStatic				m_wndImage;

// Operations
public:

// Overrides
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

// Implementation
public:

	virtual BOOL OnMessage(const CMessage& message);
	virtual BOOL OnConnect(INT nErrorCode);

	BOOL GetPassport(LPCTSTR lpf,LPCTSTR user,LPCTSTR pass,LPTSTR token);

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()

	afx_msg VOID OnPaint();
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg VOID OnSignIn();
public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	virtual void RecalcLayout(BOOL bNotify = TRUE);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};