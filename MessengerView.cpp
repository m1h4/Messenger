#include "Global.h"
#include "Messenger.h"
#include "MessengerView.h"
#include "MessengerFrame.h"
#include "MessengerAuthentication.h"

#import <msxml6.dll>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CMessengerView

IMPLEMENT_DYNCREATE(CMessengerView, CMessengerWnd)

// CMessengerView Message Map

BEGIN_MESSAGE_MAP(CMessengerView, CMessengerWnd)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_CTLCOLOR()
	ON_COMMAND(ID_BUTTON_SIGN_IN, &CMessengerView::OnSignIn)
END_MESSAGE_MAP()

CMessengerView::CMessengerView()
{
}

CMessengerView::~CMessengerView()
{
}

HBRUSH CMessengerView::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CMessengerWnd::OnCtlColor(pDC, pWnd, nCtlColor);

	// Give static controls transparent background color
	if(nCtlColor == CTLCOLOR_STATIC)
	{
		//pDC->SetBkMode(TRANSPARENT);
		pDC->SetBkColor(RGB(255,255,255));

		return (HBRUSH)GetStockObject(/*NULL_BRUSH*/ WHITE_BRUSH);
	}

   return hbr;
}

BOOL CMessengerView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;

	cs.dwExStyle |= WS_EX_CLIENTEDGE|WS_EX_CONTROLPARENT;
	cs.style &= ~WS_BORDER;
	cs.style |= WS_CLIPCHILDREN;
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, ::LoadCursor(NULL, IDC_ARROW), (HBRUSH)GetStockObject(WHITE_BRUSH), NULL);

	return TRUE;
}

VOID CMessengerView::OnPaint() 
{
	CPaintDC dc(this);

	CRect rect;
	GetClientRect(rect);

	UINT size = 14;

	CFont font1;
	font1.CreateFont(size,0,0,0,FW_BOLD,FALSE,FALSE,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,TEXT("Tahoma"));

	CFont font2;
	font2.CreateFont(size,0,0,0,FW_NORMAL,FALSE,FALSE,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,TEXT("Tahoma"));

	LPCTSTR user = TEXT("Microsoft Network");
	LPCTSTR status = TEXT(" (Offline)");

	HGDIOBJ old = dc.SelectObject(font1);
	CRect rc;
	dc.DrawText(user,rc,DT_SINGLELINE|DT_CALCRECT);
	dc.DrawText(user,CRect(5,5,rect.right-5,5+size),DT_SINGLELINE|DT_END_ELLIPSIS);
	dc.SelectObject(font2);
	dc.DrawText(status,CRect(5+rc.Width(),5,rect.right-5,5+size),DT_SINGLELINE|DT_END_ELLIPSIS);
	dc.SelectObject(old);

	dc.SelectObject(GetStockObject(DC_PEN));
	dc.SetDCPenColor(RGB(200,200,200));
	dc.MoveTo(2,5+size+2);
	dc.LineTo(rect.right-2,5+size+2);
}


VOID CMessengerView::OnSignIn()
{
	//Connect(TEXT("messenger.hotmail.com:1863"));

	CMessengerAuthentication auth;

	auth.GetAuthenticationKey(NULL,NULL,NULL,NULL);
}

BOOL CMessengerView::OnConnect(INT nErrorCode)
{
	if(!CMessengerWnd::OnConnect(nErrorCode))
		return FALSE;

	CString user;

	m_wndComboUser.GetWindowText(user);

	SendMessage(TEXT("VER 1 MSNP8 CVR0"));
	SendMessage(TEXT("CVR 2 0x0409 win 4.10 i386 MSNMSGR 5.0.0544 MSMSGS ") + user);
	SendMessage(TEXT("USR 3 TWN I ") + user);

	return TRUE;
}

BOOL CMessengerView::OnMessage(const CMessage& message)
{
	CMessengerWnd::OnMessage(message);

	if(!lstrcmpA(message.GetCommand(),"XFR"))
	{
		Disconnect();	// We will already be disconnected

		Connect(CString(message.GetParameter(1)));
	}
	else if(!lstrcmpA(message.GetCommand(),"USR") && message.GetTransaction() == 3)
	{
		TCHAR token[2048];

		CString user;
		CString pass;

		m_wndComboUser.GetWindowText(user);
		m_wndEditPass.GetWindowText(pass);

		CoInitialize(NULL);
		GetPassport(CString(message.GetParameter(2)),user,pass,token);
		CoUninitialize();

		TCHAR message[4096];
		wsprintf(message,TEXT("USR 4 TWN S %s"), token);
		SendMessage(message);
		SendMessage(TEXT("SYN 5 0"));
		SendMessage(TEXT("CHG 6 BSY 44"));
	}
	else if(!lstrcmpA(message.GetCommand(),"CHL"))
	{
		LPCSTR clientstring = "msmsgs@msnmsgr.com";
		LPCSTR clientid = "Q1P7W2E4J9R8U3S5";

		// Challenge
		CHAR source[512];

		lstrcpyA(source, message.GetParameter(0));
		lstrcatA(source, clientid);

		LPCSTR hash = HashString(source);

		TCHAR message[512];
		wsprintf(message,TEXT("QRY 10 %S %d"), clientstring, lstrlenA(hash));
		SendMessage(message);
		SendRawMessage(hash);
	}

	return TRUE;
}

BOOL CMessengerView::GetPassport(LPCTSTR lpf,LPCTSTR user, LPCTSTR pass,LPTSTR token)
{
	MSXML2::IXMLHTTPRequestPtr request;

	if(FAILED(request.CreateInstance(__uuidof(MSXML2::XMLHTTP60))))
	{
		TRACE(TEXT("Failed to create http request object.\n"));
		return FALSE;
	}

	LPCTSTR login = TEXT("https://nexus.passport.com/rdr/pprdr.asp");

	request->open(L"GET", login, VARIANT_FALSE);
	try { request->send(); } catch(...)
	{
		TRACE(TEXT("Failed to send login request.\n"));
		return FALSE;
	}

	if(request->status != 200)
	{
		TRACE(TEXT("Request failed with %s.\n%s\n"), (LPCWSTR)request->statusText, (LPCWSTR)request->responseText);
		return FALSE;
	}

	_bstr_t urls = request->getResponseHeader(TEXT("PassportURLs"));

	LPCTSTR startoflogin = TEXT("DALogin=");

	LPCTSTR auth = wcsstr((LPCWSTR)urls,startoflogin);
	if(!auth)
		return FALSE;

	auth += lstrlen(startoflogin);

	((LPWSTR)wcsstr(auth, TEXT(",")))[0] = 0;

	if(FAILED(request.CreateInstance(__uuidof(MSXML2::XMLHTTP60))))
	{
		TRACE(TEXT("Failed to create http request object.\n"));
		return FALSE;
	}

	TCHAR authorization[4096];

	wsprintf(authorization, TEXT("Passport1.4 OrgVerb=GET,OrgURL=http%3A%2F%2Fmessenger%2Emsn%2Ecom,sign-in=%s,pwd=%s,%s"), user, pass, lpf);

	request->open(L"GET", _bstr_t(TEXT("https://")) + auth, VARIANT_FALSE);
	request->setRequestHeader(TEXT("Authorization"), authorization);
	//request->setRequestHeader(TEXT("Host"), TEXT(""));
	try { request->send(); } catch(...)
	{
		TRACE(TEXT("Failed to send login request.\n"));
		return FALSE;
	}

	if(request->status != 200)
	{
		TRACE(TEXT("Request failed with %s.\n%s\n"), (LPCWSTR)request->statusText, (LPCWSTR)request->responseText);
		return FALSE;
	}

	_bstr_t authinfo = request->getResponseHeader(TEXT("Authentication-Info"));

	LPCTSTR startoftoken = TEXT("from-PP='");

	LPCTSTR authtoken = wcsstr((LPWSTR)authinfo,startoftoken);
	if(!authtoken)
		return FALSE;

	authtoken += lstrlen(startoftoken);

	while(authtoken[0] && authtoken[0] != TEXT('\''))
		token++[0] = authtoken++[0];

	token[0] = 0;

	return TRUE;
}
BOOL CMessengerView::PreTranslateMessage(MSG* pMsg)
{
	if(IsDialogMessage(pMsg))
		return TRUE;

	return CMessengerWnd::PreTranslateMessage(pMsg);
}
int CMessengerView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMessengerWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_wndImage.Create(NULL,WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE,CRect(0,0,0,0),this);
	m_wndImage.SetBitmap(LoadBitmap(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDB_MESSENGER)));

	m_wndComboUser.CreateEx(WS_EX_CLIENTEDGE,TEXT("combobox"),NULL,WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN,CRect(0,0,0,0),this, ID_EDIT_USER);
	m_wndComboUser.SendMessage(WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT));
	m_wndComboUser.AddString(TEXT("mihovilic@gmail.com"));
	
	m_wndEditPass.CreateEx(WS_EX_CLIENTEDGE,TEXT("edit"),NULL,WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD | WS_TABSTOP,CRect(0,0,0,0),this, ID_EDIT_PASSWORD);
	m_wndEditPass.SendMessage(WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT));

	m_wndComboStatus.CreateEx(WS_EX_CLIENTEDGE,TEXT("combobox"),NULL,WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,CRect(0,0,0,0),this,ID_COMBOBOX_STATUS);
	m_wndComboStatus.SendMessage(WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT));
	m_wndComboStatus.AddString(TEXT("Online"));
	m_wndComboStatus.AddString(TEXT("Away"));
	m_wndComboStatus.AddString(TEXT("Busy"));
	m_wndComboStatus.AddString(TEXT("Appear Offline"));
	m_wndComboStatus.SetCurSel(0);
	
	m_wndStaticUser.Create(TEXT("Email:"),WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS,CRect(0,0,0,0),this);
	m_wndStaticUser.SendMessage(WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT));
	
	m_wndStaticPass.Create(TEXT("Password:"),WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS,CRect(0,0,0,0),this);
	m_wndStaticPass.SendMessage(WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT));

	m_wndStaticStatus.Create(TEXT("Status:"),WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS,CRect(0,0,0,0),this);
	m_wndStaticStatus.SendMessage(WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT));

	m_wndSignIn.Create(TEXT("Sign In"),BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,CRect(0,0,0,0),this, ID_BUTTON_SIGN_IN);
	m_wndSignIn.SendMessage(WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT));

	m_wndComboUser.SetFocus();

	return 0;
}

void CMessengerView::RecalcLayout(BOOL bNotify)
{
	CRect rect;
	GetClientRect(rect);

	//ULONG offset = 30;
	ULONG offset = max(30,rect.Height()/2);
	ULONG picheight = min(200, offset - 30);

	m_wndImage.SetWindowPos(NULL,rect.left + 10, rect.top + offset - picheight, rect.right - 10 - 10, picheight, SWP_NOZORDER);
	m_wndStaticUser.SetWindowPos(NULL,rect.left + 10,rect.top + offset + 15, rect.right - 20, 15,SWP_NOZORDER);
	m_wndComboUser.SetWindowPos(NULL,rect.left + 10,rect.top + offset + 30,rect.right - 20, 19,SWP_NOZORDER);
	m_wndStaticPass.SetWindowPos(NULL,rect.left + 10,rect.top + offset + 55, rect.right - 20, 15,SWP_NOZORDER);
	m_wndEditPass.SetWindowPos(NULL,rect.left + 10,rect.top + offset + 70,rect.right - 20, 19,SWP_NOZORDER);
	m_wndStaticStatus.SetWindowPos(NULL,rect.left + 10,rect.top + offset + 95,rect.right - 20,15,SWP_NOZORDER);
	m_wndComboStatus.SetWindowPos(NULL,rect.left + 10,rect.top + offset + 110,rect.right - 20,19,SWP_NOZORDER);
	m_wndSignIn.SetWindowPos(NULL,rect.right - 10 - 75,rect.top + offset + 145,75,22,SWP_NOZORDER);
}
