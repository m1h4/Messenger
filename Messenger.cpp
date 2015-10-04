#include "Global.h"
#include "afxwinappex.h"
#include "Messenger.h"
#include "MessengerFrame.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMessengerApp

BEGIN_MESSAGE_MAP(CMessengerApp, CWinApp)
	ON_COMMAND(ID_APP_ABOUT, &CMessengerApp::OnAppAbout)
END_MESSAGE_MAP()


// CMessengerApp construction

CMessengerApp::CMessengerApp()
{

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

// The one and only CMessengerApp object

CMessengerApp theApp;


// CMessengerApp initialization

BOOL CMessengerApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	// Standard initialization
	SetRegistryKey(TEXT("Miha Software"));

	// To create the main window, this code creates a new frame window
	// object and then sets it as the application's main window object
	CMessengerFrame* pFrame = new CMessengerFrame;
	if (!pFrame)
		return FALSE;
	m_pMainWnd = pFrame;
	// create and load the frame with its resources
	pFrame->LoadFrame(IDR_MAINFRAME,WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, NULL, NULL);

	// The one and only window has been initialized, so show and update it
	pFrame->ShowWindow(SW_SHOW);
	pFrame->UpdateWindow();
	// call DragAcceptFiles only if there's a suffix
	//  In an SDI app, this should occur after ProcessShellCommand

	return TRUE;
}


// CMessengerApp message handlers




// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

// App command to run the dialog
void CMessengerApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

// CMessengerApp message handlers



