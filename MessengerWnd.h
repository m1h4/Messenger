#pragma once

// Network connection states
#define MESSENGER_NETWORK_DISCONNECTED		0
#define MESSENGER_NETWORK_RESOLVING			1
#define MESSENGER_NETWORK_CONNECTING		2
#define MESSENGER_NETWORK_CONNECTED			3
#define MESSENGER_NETWORK_DISCONNECTING		4

// Network messages
#define WM_RESOLVE_NOTIFY	(WM_USER+1)
#define WM_SOCKET_NOTIFY	(WM_USER+2)

// Network buffers
#define MESSENGER_RECEIVE_BUFFER	4096
#define MESSENGER_SEND_BUFFER		4096
#define MESSENGER_IN_BUFFER			16384
#define MESSENGER_OUT_BUFFER		16384

#define MESSENGER_MESSAGE_MAX_PARAMETERS 32

#define MESSENGER_DEFAULT_PORT 1863

// Class represents a single protocol message
class CMessage
{
public:
	CMessage();
	CMessage(LPCSTR raw);
	~CMessage();

	BOOL SetMessage(LPCSTR raw);
	VOID DeleteMessage();

	LPCSTR GetCommand() const { return m_Command; }
	ULONG GetTransaction() const { return m_Transaction; }
	LPCSTR GetParameter(ULONG index) const { ASSERT(index < m_ParameterCount);  return m_Parameters[index]; }

	LPCSTR GetMessage() const { return m_Message; }
	ULONG GetMessageLength() const { return lstrlenA(m_Message); }

protected:
	LPSTR m_Message;
	LPSTR m_Command;
	ULONG m_Transaction;
	ULONG m_ParameterCount;
	LPSTR m_Parameters[MESSENGER_MESSAGE_MAX_PARAMETERS];
};

// Class represents a network buffer
class CBuffer
{
public:
	CBuffer(ULONG size);
	~CBuffer();

public:
	BOOL AppendRaw(LPCSTR raw);
	BOOL RemoveRaw(ULONG count);

	BOOL AppendMessage(const CMessage& message);
	BOOL RemoveMessage(CMessage& message);

	LPCSTR GetRaw() const { return m_Buffer; }
	ULONG GetRawLength() const { return lstrlenA(m_Buffer); }
	ULONG GetSize() const { return m_Size; }

	VOID Clear() { ASSERT(m_Buffer); m_Buffer[0] = 0; }

protected:
	LPSTR m_Buffer;
	ULONG m_Size;
};

// CMessengerWnd window

class CMessengerWnd : public CWnd
{
	DECLARE_DYNCREATE(CMessengerWnd);

// Construction
public:
	CMessengerWnd();
	virtual ~CMessengerWnd();

// Attributes
public:

// Operations
public:

// Implementation
public:
	HANDLE m_Worker;
	ULONG m_WorkerId;
	
	SOCKET m_Socket;
	
	ULONG m_State;

	CBuffer m_InBuffer;
	CBuffer m_OutBuffer;

// Functions
public:

// Communication functions
	INT SendRawMessage(LPCSTR message);
	INT SendFormatedMessage(LPCTSTR format,...);
	INT SendMessage(LPCTSTR message);
	INT SendMessage(const CMessage& message);

// Connection functions
	BOOL Connect(LPCTSTR address);
	BOOL Disconnect();
	BOOL Close();

// Overridable Handlers (overrider must first let the parent process the message)
	virtual BOOL OnReceive(INT nErrorCode);
	virtual BOOL OnConnect(INT nErrorCode);
	virtual BOOL OnClose(INT nErrorCode);
	virtual BOOL OnResolved(INT nErrorCode, PADDRINFOT info);
	virtual BOOL OnWrite(INT nErrorCode);
	virtual BOOL OnMessage(const CMessage& message);

// Resolver worker thread
	ULONG WorkerThread();

// Message Map
protected:
	DECLARE_MESSAGE_MAP()

	afx_msg LRESULT OnResolveNotify(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnSocketNotify(WPARAM wParam, LPARAM lParam);
};

static unsigned char PADDING[64] =
{
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

class CHash
{
public:
	CHash();

public:
	VOID	Update(PBYTE chInput,ULONG nInputLenth);
	VOID	Finalize();
	PBYTE	Digest() { return m_Digest; }

private:
	VOID	Transform(PBYTE block);
	VOID	Encode(PBYTE dest, PULONG src, ULONG nLength);
	VOID	Decode(PULONG dest, PBYTE src, ULONG nLength);


	inline	ULONG	RotateLeft(ULONG x, ULONG n) { return ((x << n) | (x >> (32-n))); }
	inline	ULONG	F(ULONG x, ULONG y, ULONG z) { return ((x & y) | (~x & z)); }
	inline  ULONG	G(ULONG x, ULONG y, ULONG z) { return ((x & z) | (y & ~z)); }
	inline  ULONG	H(ULONG x, ULONG y, ULONG z) { return (x ^ y ^ z); }
	inline  ULONG	I(ULONG x, ULONG y, ULONG z) { return (y ^ (x | ~z)); }

	inline	void	FF(ULONG& a, ULONG b, ULONG c, ULONG d, ULONG x, ULONG s, ULONG ac) { a += F(b, c, d) + x + ac; a = RotateLeft(a, s); a += b; }
	inline	void	GG(ULONG& a, ULONG b, ULONG c, ULONG d, ULONG x, ULONG s, ULONG ac) { a += G(b, c, d) + x + ac; a = RotateLeft(a, s); a += b; }
	inline	void	HH(ULONG& a, ULONG b, ULONG c, ULONG d, ULONG x, ULONG s, ULONG ac) { a += H(b, c, d) + x + ac; a = RotateLeft(a, s); a += b; }
	inline	void	II(ULONG& a, ULONG b, ULONG c, ULONG d, ULONG x, ULONG s, ULONG ac) { a += I(b, c, d) + x + ac; a = RotateLeft(a, s); a += b; }

private:
	ULONG		m_State[4];
	ULONG		m_Count[2];
	BYTE		m_Buffer[64];
	BYTE		m_Digest[16];
	BYTE		m_Finalized;
};

LPCSTR HashString(LPSTR string);