#include "Global.h"
#include "Messenger.h"

#include "MessengerFrame.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMessengerFrame

IMPLEMENT_DYNAMIC(CMessengerFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMessengerFrame, CFrameWnd)
	ON_WM_CREATE()
	ON_WM_SETFOCUS()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
};

// CMessengerFrame construction/destruction

CMessengerFrame::CMessengerFrame()
{
}

CMessengerFrame::~CMessengerFrame()
{
}

int CMessengerFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if(CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	// Create a view to occupy the client area of the frame
	if(!m_wndView.Create(NULL,NULL,AFX_WS_DEFAULT_VIEW,CRect(0,0,0,0),this,AFX_IDW_PANE_FIRST))
	{
		TRACE(TEXT("Failed to create view window.\n"));
		return -1;
	}

	if(!m_wndStatusBar.Create(this))
	{
		TRACE(TEXT("Failed to create status bar.\n"));
		return -1;
	}

	m_wndStatusBar.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT));

	// Create the tray notification icon
	CreateTray();

	return 0;
}

BOOL CMessengerFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if(!CFrameWnd::PreCreateWindow(cs))
		return FALSE;

	// Load window placement data from registry
	cs.x = AfxGetApp()->GetProfileInt(TEXT("Window"),TEXT("x"), CW_USEDEFAULT);
	cs.y = AfxGetApp()->GetProfileInt(TEXT("Window"),TEXT("y"), CW_USEDEFAULT);
	cs.cx = AfxGetApp()->GetProfileInt(TEXT("Window"),TEXT("cx"), 220);;
	cs.cy = AfxGetApp()->GetProfileInt(TEXT("Window"),TEXT("cy"), 500);;

	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.lpszClass = AfxRegisterWndClass(0,0,0,LoadIcon(AfxGetInstanceHandle(),MAKEINTRESOURCE(IDR_MAINFRAME)));

	return TRUE;
}

// CMessengerFrame diagnostics

#ifdef _DEBUG
void CMessengerFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMessengerFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}
#endif //_DEBUG


// CMessengerFrame message handlers

void CMessengerFrame::OnSetFocus(CWnd* /*pOldWnd*/)
{
	// forward focus to the view window
	m_wndView.SetFocus();
}

BOOL CMessengerFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// let the view have first crack at the command
	if(m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return TRUE;

	// otherwise, do default handling
	return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CMessengerFrame::OnDestroy()
{
	// Store window placement data
	CRect rect;
	AfxGetMainWnd()->GetWindowRect(rect);

	AfxGetApp()->WriteProfileInt(TEXT("Window"),TEXT("x"),rect.left);
	AfxGetApp()->WriteProfileInt(TEXT("Window"),TEXT("y"),rect.top);
	AfxGetApp()->WriteProfileInt(TEXT("Window"),TEXT("cx"),rect.Width());
	AfxGetApp()->WriteProfileInt(TEXT("Window"),TEXT("cy"),rect.Height());

	// Remove tray icon
	DestroyTray();

	CFrameWnd::OnDestroy();
}

void CMessengerFrame::RecalcLayout(BOOL bNotify)
{
	CFrameWnd::RecalcLayout(bNotify);

	// Pass the layout reconfiguration notification to the view
	if(IsWindow(m_wndView.GetSafeHwnd()))
		m_wndView.RecalcLayout(bNotify);
}


BOOL CMessengerFrame::CreateTray(void)
{
	NOTIFYICONDATA data;
	ZeroMemory(&data,sizeof(data));
	data.cbSize = sizeof(data);
	data.hWnd = GetSafeHwnd();
	data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
	data.uCallbackMessage = WM_USER;
	data.hIcon = AfxGetApp()->LoadIcon(MAKEINTRESOURCE(IDR_MAINFRAME));
	lstrcpy(data.szTip,TEXT("Messenger"));

	if(!Shell_NotifyIcon(NIM_ADD,&data))
	{
		TRACE(TEXT("Failed to add notification icon.\n"));
		return FALSE;
	}

	return TRUE;
}

BOOL CMessengerFrame::DestroyTray(void)
{
	NOTIFYICONDATA data;
	ZeroMemory(&data,sizeof(data));
	data.cbSize = sizeof(data);
	data.hWnd = GetSafeHwnd();

	if(!Shell_NotifyIcon(NIM_DELETE,&data))
	{
		TRACE(TEXT("Failed to add notification icon.\n"));
		return FALSE;
	}

	return TRUE;
}
