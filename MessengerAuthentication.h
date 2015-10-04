#pragma once

#include <schannel.h>
#define SECURITY_WIN32
#include <security.h>

class CHttpRequest
{
public:
	CHttpRequest() {}
	~CHttpRequest() {}

	CString Compile();

	void SetMethod(LPCTSTR method) { m_Method = method; }
	LPCTSTR GetMethod(LPCTSTR method) { return m_Method; }
	void SetResource(LPCTSTR resource) { m_Resource = resource; }
	LPCTSTR GetResource() { return m_Resource; }
	void SetHeader(LPCTSTR name,LPCTSTR value) { m_Headers[name] = value; }
	LPCTSTR GetHeader(LPCTSTR name) { return m_Headers[name]; }

protected:
	CString				m_Method;
	CString				m_Resource;
	CMapStringToString	m_Headers;
};

class CHttpResponse
{
public:
	CHttpResponse() {}
	~CHttpResponse() {}

	void Parse(LPCTSTR response);

	void SetStatus(LPCTSTR status) { m_Status = status; }
	LPCTSTR GetStatus() { return m_Status; }
	void SetHeader(LPCTSTR name,LPCTSTR value) { m_Headers[name] = value; }
	LPCTSTR GetHeader(LPCTSTR name) { return m_Headers[name]; }

protected:
	CString				m_Status;
	CMapStringToString	m_Headers;
};

// CMessengerAuthentication command target

class CMessengerAuthentication : public CObject
{
	DECLARE_DYNAMIC(CMessengerAuthentication);

public:
	CMessengerAuthentication();
	virtual ~CMessengerAuthentication();

protected:
	SECURITY_STATUS SecureCreateCredentials(PCredHandle phCreds);
	ULONG ConnectServer(LPCTSTR pszServerName,LPCTSTR pszPort,SOCKET* pSocket);
	ULONG SecureDisconnectServer(SOCKET hSocket,PCredHandle phCreds,CtxtHandle* phContext);
	SECURITY_STATUS SecureClientHandshakeLoop(SOCKET hSocket,PCredHandle phCreds,CtxtHandle* phContext,BOOL fDoInitialRead,SecBuffer* pExtraData);
	SECURITY_STATUS SecurePerformClientHandshake(SOCKET hSocket,PCredHandle phCreds,LPCTSTR pszServerName,CtxtHandle* phContext,SecBuffer* pExtraData);
	ULONG SecureSendCommand(SOCKET hSocket, CredHandle hCreditentials, CtxtHandle hContext, LPCSTR command);
	ULONG SocketDataReady(SOCKET hSocket);
	ULONG SecureReceiveCommand(SOCKET hSocket, CredHandle hCreditentials, CtxtHandle hContext,LPBYTE buffer,ULONG size);

public:
	bool GetAuthenticationKey(LPCTSTR lpf,LPCTSTR user, LPCTSTR pass,LPTSTR token);
};