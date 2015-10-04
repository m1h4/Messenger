#include "Global.h"
#include "Messenger.h"
#include "MessengerWnd.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Function used to call the worker thread member function of messengerwnd
ULONG WINAPI WorkerThreadProxy(CMessengerWnd& context)
{
	return context.WorkerThread();
}

#pragma region CMessage

CMessage::CMessage() :
 m_Message(NULL),
 m_Command(NULL),
 m_Transaction(-1),
 m_ParameterCount(NULL)
{
}

CMessage::CMessage(LPCSTR raw) :
 m_Message(NULL),
 m_Command(NULL),
 m_Transaction(-1),
 m_ParameterCount(NULL)
{
	SetMessage(raw);
}

CMessage::~CMessage()
{
	DeleteMessage();
}

VOID CMessage::DeleteMessage()
{
	while(m_ParameterCount)
	{
		--m_ParameterCount;

		delete[] m_Parameters[m_ParameterCount];
		m_Parameters[m_ParameterCount] = NULL;
	}

	delete[] m_Message;
	m_Message = NULL;

	delete[] m_Command;
	m_Command = NULL;
	
	m_Transaction = -1;
}

BOOL CMessage::SetMessage(LPCSTR raw)
{
	ASSERT(raw);

	DeleteMessage();
	
	ULONG length = lstrlenA(raw);
	
	m_Message = new CHAR[length + 1];
	
	lstrcpyA(m_Message,raw);

	// Parse the message into parts
	ULONG start = 0, seek = 0;

	while(m_Message[seek] && m_Message[seek] == ' ')
		++seek;

	start = seek;

	while(m_Message[seek] && m_Message[seek] != ' ')
		++seek;

	m_Command = new CHAR[seek - start + 1];

	lstrcpynA(m_Command, m_Message + start, seek - start + 1);

	while(m_Message[seek] && m_Message[seek] == ' ')
		++seek;

	start = seek;

	while(m_Message[seek] && m_Message[seek] != ' ')
		++seek;

	m_Transaction = strtoul(m_Message + start, NULL, 10);

	while(true)
	{
		while(m_Message[seek] && m_Message[seek] == ' ')
			++seek;

		start = seek;

		while(m_Message[seek] && m_Message[seek] != ' ')
			++seek;

		if(seek - start == 0)
			break;

		if(m_ParameterCount == MESSENGER_MESSAGE_MAX_PARAMETERS)
		{
			TRACE(TEXT("Command has more parameters than currently supported.\n"));
			break;
		}

		m_Parameters[m_ParameterCount] = new CHAR[seek - start + 1];

		lstrcpynA(m_Parameters[m_ParameterCount], m_Message + start, seek - start + 1);

		++m_ParameterCount;
	}
	
	return TRUE;
}
#pragma endregion

#pragma region CBuffer
CBuffer::CBuffer(ULONG size) :
 m_Size(size)
{
	ASSERT(m_Size);

	m_Buffer = new CHAR[m_Size];
	ASSERT(m_Buffer);

	m_Buffer[0] = '\0';
}

CBuffer::~CBuffer()
{
	delete[] m_Buffer;
	m_Buffer = NULL;
	m_Size = NULL;
}

BOOL CBuffer::AppendRaw(LPCSTR raw)
{
	ASSERT(m_Buffer);
	ASSERT(raw);
	ASSERT(m_Size > lstrlenA(m_Buffer) + lstrlenA(raw));

	// Append the message
	lstrcatA(m_Buffer, raw);

	return TRUE;
}

BOOL CBuffer::RemoveRaw(ULONG count)
{
	// Remove the message from the buffer
	MoveMemory(m_Buffer, m_Buffer + count, lstrlenA(m_Buffer + count) + 1);

	return TRUE;
}

BOOL CBuffer::AppendMessage(const CMessage& message)
{
	LPCSTR terminator = "\r\n";

	ASSERT(m_Buffer);
	ASSERT(m_Size > lstrlenA(m_Buffer) + lstrlenA(message.GetMessage()) + lstrlenA(terminator));

	// Append the message
	lstrcatA(m_Buffer, message.GetMessage());

	// Append the message terminator
	lstrcatA(m_Buffer, terminator);

	return TRUE;
}

BOOL CBuffer::RemoveMessage(CMessage& message)
{
	ASSERT(m_Buffer);

	ULONG search = 0;

	// Find message terminator
	while(m_Buffer[search] && m_Buffer[search] != '\r' && m_Buffer[search + 1] != '\n')
		++search;

	// Check if no full message found
	if(!m_Buffer[search])
		return FALSE;

	// Store the message
	m_Buffer[search] = 0;
	message.SetMessage(m_Buffer);

	// Remove the message from the buffer
	RemoveRaw(search + 2);

	return TRUE;
}
#pragma endregion

// CMessengerWnd

IMPLEMENT_DYNCREATE(CMessengerWnd, CWnd)

BEGIN_MESSAGE_MAP(CMessengerWnd, CWnd)
	ON_MESSAGE(WM_RESOLVE_NOTIFY, OnResolveNotify)
	ON_MESSAGE(WM_SOCKET_NOTIFY, OnSocketNotify)
END_MESSAGE_MAP()

CMessengerWnd::CMessengerWnd() :
 m_Worker(INVALID_HANDLE_VALUE),
 m_WorkerId(NULL),
 m_InBuffer(MESSENGER_IN_BUFFER),
 m_OutBuffer(MESSENGER_OUT_BUFFER),
 m_Socket(INVALID_SOCKET),
 m_State(MESSENGER_NETWORK_DISCONNECTED)
{
	// Create the worker thread
	m_Worker = CreateThread(NULL,NULL,(LPTHREAD_START_ROUTINE)WorkerThreadProxy,this,NULL,&m_WorkerId);
	ASSERT(m_Worker != INVALID_HANDLE_VALUE);
}

CMessengerWnd::~CMessengerWnd()
{
	// Close open connection
	Close();

	// Tell the resolve thread to shutdown and wait for it
	PostThreadMessage(m_WorkerId,WM_QUIT,NULL,NULL);
	WaitForSingleObject(m_Worker,INFINITE);
	CloseHandle(m_Worker);

	m_Worker = INVALID_HANDLE_VALUE;
	m_WorkerId = NULL;
}

ULONG CMessengerWnd::WorkerThread(VOID)
{
	TRACE(TEXT("Resolve thread started.\n"));

	MSG message;
	while(GetMessage(&message,NULL,NULL,NULL))
	{
		if(message.message == WM_NULL)
			continue;
		else if(message.message == WM_RESOLVE_NOTIFY)
		{
			TRACE(TEXT("Resolve thread got resolve request for %s:%s.\n"), message.wParam, message.lParam);

			ADDRINFOT hints;
			ZeroMemory(&hints,sizeof(hints));

			// Set the hints host address structure to out specifications
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;

			PADDRINFOT info = NULL;

			INT nErrorCode = GetAddrInfo((PCWSTR)message.wParam,(PCWSTR)message.lParam,&hints,&info);
			if(nErrorCode)
			{
				// Free the sent host name and port strings
				delete[] (PCWSTR)message.lParam;
				delete[] (PCWSTR)message.wParam;

				TRACE(TEXT("Resolve thread failed to resolve %s:%s.\n"), message.wParam, message.lParam);

				PostMessage(WM_RESOLVE_NOTIFY, nErrorCode, NULL);
			}
			else
			{
				TCHAR host[256];
				ULONG length = _countof(host);

				WSAAddressToString(info->ai_addr,info->ai_addrlen,NULL,host,&length);

				TRACE(TEXT("Resolve thread resolved %s:%s into %s.\n"), message.wParam, message.lParam, host);

				if(info->ai_next)
					TRACE(TEXT("Info: Resolved host %s:%s has multiple addresses.\n"), message.wParam, message.lParam);

				// Free the sent host name and port strings
				delete[] (PCWSTR)message.lParam;
				delete[] (PCWSTR)message.wParam;

				PostMessage(WM_RESOLVE_NOTIFY, NULL, (LPARAM)info);
			}
		}
		else
		{
			// We got some unknown message
			TRACE(TEXT("Resolve thread got unsupported message %d.\n"), message.message);
		}
	}

	// We were signaled to exit
	TRACE(TEXT("Resolve thread exiting...\n"));
	return 0;
}

INT CMessengerWnd::SendRawMessage(LPCSTR message)
{
	ASSERT(m_Socket != INVALID_SOCKET);
	ASSERT(m_State >= MESSENGER_NETWORK_CONNECTED);

	// Append the data to the out buffer
	m_OutBuffer.AppendRaw(message);

	// Check if we can send some data immediatly
	if(m_OutBuffer.GetRawLength())
	{
		// Try to send data stored in the out buffer
		INT sent = send(m_Socket, m_OutBuffer.GetRaw(), m_OutBuffer.GetRawLength(), 0);

		{
			CHAR buffer[2048];

			CopyMemory(buffer, m_OutBuffer.GetRaw(), sent);

			buffer[sent] = 0;

			TRACE(TEXT("Sent: %S [%d/%d]\n"), buffer,sent, m_OutBuffer.GetRawLength());
		}
		
		// Remove the sent part of the buffer
		m_OutBuffer.RemoveRaw(sent);
	}

	return TRUE;
}

INT CMessengerWnd::SendMessage(const CMessage& message)
{
	ASSERT(m_Socket != INVALID_SOCKET);
	ASSERT(m_State >= MESSENGER_NETWORK_CONNECTED);

	// Append the data to the out buffer
	m_OutBuffer.AppendMessage(message);

	// Check if we can send some data immediatly
	if(m_OutBuffer.GetRawLength())
	{
		// Try to send data stored in the out buffer
		INT sent = send(m_Socket, m_OutBuffer.GetRaw(), m_OutBuffer.GetRawLength(), 0);

		{
			CHAR buffer[2048];

			CopyMemory(buffer, m_OutBuffer.GetRaw(), sent);

			buffer[sent] = 0;

			TRACE(TEXT("Sent: %S [%d/%d]\n"), buffer,sent, m_OutBuffer.GetRawLength());
		}
		
		// Remove the sent part of the buffer
		m_OutBuffer.RemoveRaw(sent);
	}

	return TRUE;
}

BOOL CMessengerWnd::Connect(LPCTSTR address)
{
	ASSERT(m_Worker != INVALID_HANDLE_VALUE);
	ASSERT(m_Socket == INVALID_SOCKET);
	ASSERT(m_State == MESSENGER_NETWORK_DISCONNECTED);
	ASSERT(address && lstrlen(address));

	// Split the address and the port number (if one is specified
	ULONG start = 0, seek = 0;

	while(address[seek] && address[seek] == TEXT(' '))
		++seek;

	start = seek;

	while(address[seek] && address[seek] != TEXT(':'))
		++seek;

	// Check if address specified
	if(seek - start == 0)
		return FALSE;

	LPTSTR paddress = new TCHAR[seek - start + 1];
	ASSERT(paddress);

	lstrcpyn(paddress,address + start,seek - start + 1);

	if(address[seek] == TEXT(':'))
		++seek;

	start = seek;

	while(address[seek] && address[seek] != TEXT(' '))
		++seek;
	
	LPTSTR pport = NULL;

	// Check if no port specified (use default port)
	if(start - seek)
	{
		pport = new TCHAR[seek - start + 1];
		ASSERT(pport);

		lstrcpyn(pport,address + start,seek - start + 1);
	}
	else
	{
		pport = new TCHAR[32];
		ASSERT(pport);

		_ultow_s(MESSENGER_DEFAULT_PORT,pport,32,10);
	}

	// Start resolve operation
	if(!PostThreadMessage(m_WorkerId,WM_RESOLVE_NOTIFY,(WPARAM)paddress,(LPARAM)pport))
	{
		delete[] paddress;
		delete[] pport;

		return FALSE;
	}

	// We are resolving
	m_State = MESSENGER_NETWORK_RESOLVING;

	return TRUE;
}

BOOL CMessengerWnd::Disconnect()
{
	ASSERT(m_Socket != INVALID_SOCKET);
	ASSERT(m_State == MESSENGER_NETWORK_CONNECTED);

	// Currently we just close the connection here
	Close();

	return TRUE;
}

BOOL CMessengerWnd::Close(VOID)
{
	// Reset our network state
	m_State = MESSENGER_NETWORK_DISCONNECTED;

	if(m_Socket != INVALID_SOCKET)
	{
		// Close the socket handle and set it to a invalid value
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;

		// Empty the network buffers
		if(m_InBuffer.GetRawLength())
			TRACE(TEXT("Warning: InBuffer not empty.\n"));

		if(m_OutBuffer.GetRawLength())
			TRACE(TEXT("Warning: OutBuffer not empty.\n"));

		m_InBuffer.Clear();
		m_OutBuffer.Clear();
	}

	TRACE(TEXT("Connection closed.\n"));

	return TRUE;
}

LRESULT CMessengerWnd::OnResolveNotify(WPARAM wParam, LPARAM lParam)
{
	OnResolved(wParam,(PADDRINFOT)lParam);

	// We must free the resolve information
	FreeAddrInfo((PADDRINFOT)lParam);

	return 0;
}

LRESULT CMessengerWnd::OnSocketNotify(WPARAM wParam, LPARAM lParam)
{
	// Get the event code from the param
	switch(WSAGETSELECTEVENT(lParam))
	{
	case FD_CONNECT:
		OnConnect(WSAGETSELECTERROR(lParam));
		break;

	case FD_CLOSE:
		OnClose(WSAGETSELECTERROR(lParam));
		break;

	case FD_READ:
		OnReceive(WSAGETSELECTERROR(lParam));
		break;

	case FD_WRITE:
		OnWrite(WSAGETSELECTERROR(lParam));
		break;

	default:
		// This should not happen
		AfxThrowNotSupportedException();
		break;
	}

	return 0;
}

INT CMessengerWnd::SendMessage(LPCTSTR message)
{
	// Get size of translate buffer
	INT bufferSize = WideCharToMultiByte(CP_UTF8,NULL,message,-1,NULL,NULL,NULL,NULL);
	if(!bufferSize)
	{
		TRACE(TEXT("Error occurred while converting string to multibyte %d.\n"),GetLastError());
		return 0;
	}

	LPSTR buffer = new CHAR[bufferSize];
	ASSERT(buffer);

	// Convert the string to multibyte, string will not be null terminated
	bufferSize = WideCharToMultiByte(CP_UTF8,NULL,message,-1,buffer,bufferSize,NULL,NULL);

	bufferSize = SendMessage(CMessage(buffer));

	delete[] buffer;

	return bufferSize;
}

INT CMessengerWnd::SendFormatedMessage(LPCTSTR format,...)
{
	TCHAR buffer[4096];

    va_list args;
    va_start(args,format);
	INT written = vswprintf_s(buffer,sizeof(buffer)/sizeof(buffer[0]),format,args);
    va_end(args);

	if(written < 0)
		return 0;

	return SendMessage(buffer);
}

BOOL CMessengerWnd::OnConnect(INT nErrorCode)
{
	if(nErrorCode)
	{
		// Make sure we are closed
		return Close();
	}

	// We are now connected
	m_State = MESSENGER_NETWORK_CONNECTED;

	TRACE(TEXT("Connected.\n"));

	return TRUE;
}

BOOL CMessengerWnd::OnResolved(INT nErrorCode,PADDRINFOT info)
{
	ASSERT(m_Socket == INVALID_SOCKET);
	ASSERT(nErrorCode || info);

	if(nErrorCode)
	{
		// Make sure we are closed
		return Close();
	}

	// Create the socket first
	m_Socket = WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,NULL,NULL);
	if(m_Socket == INVALID_SOCKET)
	{
		TRACE(TEXT("Failed to create socket %d\n"),WSAGetLastError());
		return FALSE;
	}

	// Set the events we need to receive
	if(WSAAsyncSelect(m_Socket,GetSafeHwnd(),WM_SOCKET_NOTIFY,FD_CONNECT|FD_CLOSE|FD_READ|FD_WRITE) == SOCKET_ERROR)
	{
		// Make sure the socket is closed
		Close();

		TRACE(TEXT("Failed to set socket events %d\n"),WSAGetLastError());
		return FALSE;
	}

	// Try to connect to the given address
	if(WSAConnect(m_Socket,info->ai_addr,info->ai_addrlen,NULL,NULL,NULL,NULL) == SOCKET_ERROR)
	{
		// Check if it was the blocking singal
		if(WSAGetLastError() != WSAEWOULDBLOCK)
		{
			// We seem to have stumbled upon a error
			TRACE(TEXT("Error connecting %d.\n"),WSAGetLastError());

			// Make sure the socket is closed
			Close();

			return FALSE;
		}
	}

	// We are connecting
	m_State = MESSENGER_NETWORK_CONNECTING;

	TRACE(TEXT("Connecting...\n"));

	return TRUE;
}

BOOL CMessengerWnd::OnReceive(INT nErrorCode)
{
	ASSERT(m_Socket != INVALID_SOCKET);

	if(nErrorCode)
	{
		// Make sure we are closed
		return Close();
	}

	TRACE(TEXT("Receiving...\n"));

	CHAR buffer[MESSENGER_RECEIVE_BUFFER];

	// Do recv
	INT received = recv(m_Socket, buffer, sizeof(buffer), 0);
	
	buffer[received] = 0;

	// Append the received data to our buffer
	m_InBuffer.AppendRaw(buffer);

	// Check if any whole messages received
	while(true)
	{
		CMessage message;

		if(!m_InBuffer.RemoveMessage(message))
			break;

		// Do not pass empty messages
		if(message.GetMessageLength())
			OnMessage(message);
	}

	return TRUE;
}

BOOL CMessengerWnd::OnMessage(const CMessage& message)
{
	TRACE(TEXT("Got message: \"%S\".\n"), message.GetMessage());

	return TRUE;
}

BOOL CMessengerWnd::OnWrite(INT nErrorCode)
{
	ASSERT(m_Socket != INVALID_SOCKET);

	if(nErrorCode)
	{
		// Make sure we are closed
		return Close();
	}

	TRACE(TEXT("Can send.\n"));

	if(m_OutBuffer.GetRawLength())
	{
		// Send data stored in the out buffer
		INT sent = send(m_Socket, m_OutBuffer.GetRaw(), m_OutBuffer.GetRawLength(), 0);

		TRACE(TEXT("Sent %d/%d.\n"), sent, m_OutBuffer.GetRawLength());
		
		// Remove the sent part of the buffer
		m_OutBuffer.RemoveRaw(sent);
	}

	return TRUE;
}

BOOL CMessengerWnd::OnClose(INT nErrorCode)
{
	TRACE(TEXT("Connection closing...\n"));

	// Make sure our socket is closed
	return Close();
}

LPCSTR HashString(LPSTR string)
{
	static CHAR text[128];

	CHash hash;

	hash.Update((PBYTE)string, lstrlenA(string));
	hash.Finalize();

	sprintf_s(text,_countof(text), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		hash.Digest()[0],hash.Digest()[1],hash.Digest()[2],hash.Digest()[3],
		hash.Digest()[4],hash.Digest()[5],hash.Digest()[6],hash.Digest()[7],
		hash.Digest()[8],hash.Digest()[9],hash.Digest()[10],hash.Digest()[11],
		hash.Digest()[12],hash.Digest()[13],hash.Digest()[14],hash.Digest()[15]);

	return text;
}

// Initializes a new context
CHash::CHash()
{
	ZeroMemory(m_Count,2 * sizeof(ULONG));

	m_State[0] = 0x67452301;
	m_State[1] = 0xefcdab89;
	m_State[2] = 0x98badcfe;
	m_State[3] = 0x10325476;
}

// MD5 block update operation. Continues an MD5 message-digest operation, processing another message block, and updating the context
VOID CHash::Update(PBYTE chInput, ULONG nInputLen)
{
	ULONG i, index, partLen;

	// Compute number of bytes mod 64
	index = (unsigned int)((m_Count[0] >> 3) & 0x3F);

	// Update number of bits
	if ((m_Count[0] += (nInputLen << 3)) < (nInputLen << 3))
		m_Count[1]++;

	m_Count[1] += (nInputLen >> 29);

	partLen = 64 - index;

	// Transform as many times as possible.
	if (nInputLen >= partLen)
	{
		CopyMemory( &m_Buffer[index], chInput, partLen );
		Transform(m_Buffer);

		for (i = partLen; i + 63 < nInputLen; i += 64)
			Transform(&chInput[i]);

		index = 0;
	}
	else
		i = 0;

  // Buffer remaining input
  CopyMemory( &m_Buffer[index], &chInput[i], nInputLen-i );
}

// MD5 finalization. Ends an MD5 message-digest operation, writing the message digest and zeroizing the context
VOID CHash::Finalize()
{
	BYTE bits[8];
	ULONG index, padLen;

	// Save number of bits
	Encode (bits, m_Count, 8);

	// Pad out to 56 mod 64
	index = (unsigned int)((m_Count[0] >> 3) & 0x3f);
	padLen = (index < 56) ? (56 - index) : (120 - index);
	Update(PADDING, padLen);

	// Append length (before padding)
	Update (bits, 8);

	// Store state in digest
	Encode (m_Digest, m_State, 16);

	ZeroMemory(m_Count, 2 * sizeof(ULONG));
	ZeroMemory(m_State, 4 * sizeof(ULONG));
	ZeroMemory(m_Buffer, 64 * sizeof(BYTE));
}

// MD5 basic transformation. Transforms state based on block
VOID CHash::Transform (PBYTE block)
{
  ULONG a = m_State[0], b = m_State[1], c = m_State[2], d = m_State[3], x[16];

  Decode (x, block, 64);

  // Round 1
  FF (a, b, c, d, x[ 0], S11, 0xd76aa478);
  FF (d, a, b, c, x[ 1], S12, 0xe8c7b756);
  FF (c, d, a, b, x[ 2], S13, 0x242070db);
  FF (b, c, d, a, x[ 3], S14, 0xc1bdceee);
  FF (a, b, c, d, x[ 4], S11, 0xf57c0faf);
  FF (d, a, b, c, x[ 5], S12, 0x4787c62a);
  FF (c, d, a, b, x[ 6], S13, 0xa8304613);
  FF (b, c, d, a, x[ 7], S14, 0xfd469501);
  FF (a, b, c, d, x[ 8], S11, 0x698098d8);
  FF (d, a, b, c, x[ 9], S12, 0x8b44f7af);
  FF (c, d, a, b, x[10], S13, 0xffff5bb1);
  FF (b, c, d, a, x[11], S14, 0x895cd7be);
  FF (a, b, c, d, x[12], S11, 0x6b901122);
  FF (d, a, b, c, x[13], S12, 0xfd987193);
  FF (c, d, a, b, x[14], S13, 0xa679438e);
  FF (b, c, d, a, x[15], S14, 0x49b40821);

 // Round 2
  GG (a, b, c, d, x[ 1], S21, 0xf61e2562);
  GG (d, a, b, c, x[ 6], S22, 0xc040b340);
  GG (c, d, a, b, x[11], S23, 0x265e5a51);
  GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa);
  GG (a, b, c, d, x[ 5], S21, 0xd62f105d);
  GG (d, a, b, c, x[10], S22,  0x2441453);
  GG (c, d, a, b, x[15], S23, 0xd8a1e681);
  GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8);
  GG (a, b, c, d, x[ 9], S21, 0x21e1cde6);
  GG (d, a, b, c, x[14], S22, 0xc33707d6);
  GG (c, d, a, b, x[ 3], S23, 0xf4d50d87);
  GG (b, c, d, a, x[ 8], S24, 0x455a14ed);
  GG (a, b, c, d, x[13], S21, 0xa9e3e905);
  GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8);
  GG (c, d, a, b, x[ 7], S23, 0x676f02d9);
  GG (b, c, d, a, x[12], S24, 0x8d2a4c8a);

  // Round 3
  HH (a, b, c, d, x[ 5], S31, 0xfffa3942);
  HH (d, a, b, c, x[ 8], S32, 0x8771f681);
  HH (c, d, a, b, x[11], S33, 0x6d9d6122);
  HH (b, c, d, a, x[14], S34, 0xfde5380c);
  HH (a, b, c, d, x[ 1], S31, 0xa4beea44);
  HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9);
  HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60);
  HH (b, c, d, a, x[10], S34, 0xbebfbc70);
  HH (a, b, c, d, x[13], S31, 0x289b7ec6);
  HH (d, a, b, c, x[ 0], S32, 0xeaa127fa);
  HH (c, d, a, b, x[ 3], S33, 0xd4ef3085);
  HH (b, c, d, a, x[ 6], S34,  0x4881d05);
  HH (a, b, c, d, x[ 9], S31, 0xd9d4d039);
  HH (d, a, b, c, x[12], S32, 0xe6db99e5);
  HH (c, d, a, b, x[15], S33, 0x1fa27cf8);
  HH (b, c, d, a, x[ 2], S34, 0xc4ac5665);

  // Round 4
  II (a, b, c, d, x[ 0], S41, 0xf4292244);
  II (d, a, b, c, x[ 7], S42, 0x432aff97);
  II (c, d, a, b, x[14], S43, 0xab9423a7);
  II (b, c, d, a, x[ 5], S44, 0xfc93a039);
  II (a, b, c, d, x[12], S41, 0x655b59c3);
  II (d, a, b, c, x[ 3], S42, 0x8f0ccc92);
  II (c, d, a, b, x[10], S43, 0xffeff47d);
  II (b, c, d, a, x[ 1], S44, 0x85845dd1);
  II (a, b, c, d, x[ 8], S41, 0x6fa87e4f);
  II (d, a, b, c, x[15], S42, 0xfe2ce6e0);
  II (c, d, a, b, x[ 6], S43, 0xa3014314);
  II (b, c, d, a, x[13], S44, 0x4e0811a1);
  II (a, b, c, d, x[ 4], S41, 0xf7537e82);
  II (d, a, b, c, x[11], S42, 0xbd3af235);
  II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb);
  II (b, c, d, a, x[ 9], S44, 0xeb86d391);

  m_State[0] += a;
  m_State[1] += b;
  m_State[2] += c;
  m_State[3] += d;

  ZeroMemory(x, sizeof(x));
}

// Encodes input (uint4) into output (uchar). Assumes nLength is a multiple of 4
VOID CHash::Encode(PBYTE dest, PULONG src, ULONG nLength)
{
	ULONG i, j;

	ASSERT(nLength % 4 == 0);

	for (i = 0, j = 0; j < nLength; i++, j += 4)
	{
		dest[j] = (BYTE)(src[i] & 0xff);
		dest[j+1] = (BYTE)((src[i] >> 8) & 0xff);
		dest[j+2] = (BYTE)((src[i] >> 16) & 0xff);
		dest[j+3] = (BYTE)((src[i] >> 24) & 0xff);
	}
}

// Decodes input (uchar) into output (uint4). Assumes nLength is a multiple of 4
VOID CHash::Decode(PULONG dest, PBYTE src, ULONG nLength)
{
	ULONG i, j;

	ASSERT(nLength % 4 == 0);

	for (i = 0, j = 0; j < nLength; i++, j += 4)
	{
		dest[i] = ((ULONG)src[j]) | (((ULONG)src[j+1])<<8) | (((ULONG)src[j+2])<<16) | (((ULONG)src[j+3])<<24);
	}
}