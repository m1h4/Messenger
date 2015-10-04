// MessengerAuthentication.cpp : implementation file
//

#include "Global.h"
#include "Messenger.h"
#include "MessengerAuthentication.h"

#pragma comment(lib,"secur32.lib")

#define IO_BUFFER_SIZE 8192

CString CHttpRequest::Compile()
{
	CString request = m_Method.MakeUpper() + TEXT(" ") + m_Resource + TEXT(" HTTP/1.1\r\n");

	POSITION position = m_Headers.GetStartPosition();
	while(position)
	{
		CString key,value;

		m_Headers.GetNextAssoc(position,key,value);

		request += key + TEXT(": ") + value + TEXT("\r\n");
	}

	return request + TEXT("\r\n");
}

void CHttpResponse::Parse(LPCTSTR response)
{
	AfxThrowNotSupportedException();
}

IMPLEMENT_DYNAMIC(CMessengerAuthentication, CObject);

// CMessengerAuthentication

CMessengerAuthentication::CMessengerAuthentication()
{
}

CMessengerAuthentication::~CMessengerAuthentication()
{
}

// CMessengerAuthentication member functions

bool CMessengerAuthentication::GetAuthenticationKey(LPCTSTR lpf,LPCTSTR user, LPCTSTR pass,LPTSTR token)
{
	SOCKET hSocket = INVALID_SOCKET;
	CredHandle hCreditentials = {0,0};
	CtxtHandle hContext = {0,0};
	BYTE buffer[8096];
	LPCTSTR server = TEXT("nexus.passport.com");
	LPCTSTR port = TEXT("443");

	if(ConnectServer(server, port, &hSocket))
	{
		// Failed to connect
		TRACE(TEXT("Cannon connect, failed to establish connection.\n"));
		return FALSE;
	}

	if(SecureCreateCredentials(&hCreditentials))
	{
		// Failed to create credentials
		TRACE(TEXT("Cannot connect, failed to create credientials.\n"));
		return FALSE;
	}

	if(SecurePerformClientHandshake(hSocket, &hCreditentials, server, &hContext, NULL))
	{
		// Failed to perform handshake
		TRACE(TEXT("Cannot connect, failed to perform handshake.\n"));
		return FALSE;
	}

	if(SecureSendCommand(hSocket, hCreditentials, hContext, "GET /rdr/pprdr.asp HTTP/1.0\r\n\r\n"))
	{
		TRACE(TEXT("Failed to send command.\n"));
	}

	ULONG data = SecureReceiveCommand(hSocket, hCreditentials, hContext, buffer, sizeof(buffer));
	if(!data)
	{
		TRACE(TEXT("Failed to receive command.\n"));
	}

	buffer[data] = 0;

	TRACE(TEXT("%S\n"),buffer);

	SecureDisconnectServer(hSocket, &hCreditentials, &hContext);

	return true;
}

SECURITY_STATUS CMessengerAuthentication::SecureCreateCredentials(PCredHandle phCreds)
{
	SCHANNEL_CRED schannelCred;

	ZeroMemory(&schannelCred, sizeof(schannelCred));
	schannelCred.dwVersion = SCHANNEL_CRED_VERSION;
 
	return AcquireCredentialsHandle(NULL,UNISP_NAME,SECPKG_CRED_OUTBOUND,NULL,&schannelCred,NULL,NULL,phCreds,NULL);
}

ULONG CMessengerAuthentication::ConnectServer(LPCTSTR pszServerName,LPCTSTR pszPort,SOCKET* pSocket)
{
	PADDRINFOT addr = NULL;

	TRACE(TEXT("Resolving host %s:%s...\n"),pszServerName,pszPort);
	if(GetAddrInfo(pszServerName,pszPort,NULL,&addr))
	{
		TRACE(TEXT("Resolve failed: %d\n"),WSAGetLastError());
		return 1;
	}

	*pSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(*pSocket == INVALID_SOCKET)
	{
		FreeAddrInfo(addr);
		return 1;
	}

	if(connect(*pSocket, addr->ai_addr, addr->ai_addrlen) == SOCKET_ERROR)
	{
		FreeAddrInfo(addr);
		closesocket(*pSocket);
		TRACE(TEXT("Connect failed: %d\n"),WSAGetLastError());
		return 1;
	}

	FreeAddrInfo(addr);
	return 0;
}

ULONG CMessengerAuthentication::SecureDisconnectServer(SOCKET hSocket,PCredHandle phCreds,CtxtHandle* phContext)
{
	DWORD		dwType;
	PBYTE		pbMessage;
	DWORD		cbMessage;
	DWORD		cbData;

	SecBufferDesc	OutBuffer;
	SecBuffer		OutBuffers[1];
	DWORD			dwSSPIFlags;
	DWORD			dwSSPIOutFlags;
	TimeStamp		tsExpiry;
	DWORD			Status;

	//
	// Notify schannel that we are about to close the connection.
	//

	dwType = SCHANNEL_SHUTDOWN;

	OutBuffers[0].pvBuffer = &dwType;
	OutBuffers[0].BufferType = SECBUFFER_TOKEN;
	OutBuffers[0].cbBuffer = sizeof(dwType);

	OutBuffer.cBuffers = 1;
	OutBuffer.pBuffers = OutBuffers;
	OutBuffer.ulVersion = SECBUFFER_VERSION;

	Status = ApplyControlToken(phContext, &OutBuffer);
	if(FAILED(Status)) 
	{
		DeleteSecurityContext(phContext);
		closesocket(hSocket);
		FreeCredentialsHandle(phCreds);
		return 1;
	}

	//
	// Build an SSL close notify message.
	//

	dwSSPIFlags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;

	OutBuffers[0].pvBuffer = NULL;
	OutBuffers[0].BufferType = SECBUFFER_TOKEN;
	OutBuffers[0].cbBuffer = 0;

	OutBuffer.cBuffers = 1;
	OutBuffer.pBuffers = OutBuffers;
	OutBuffer.ulVersion = SECBUFFER_VERSION;

	Status = InitializeSecurityContext(phCreds,phContext,NULL,dwSSPIFlags,0,SECURITY_NATIVE_DREP,NULL,0,phContext,&OutBuffer,&dwSSPIOutFlags,&tsExpiry);
	if(FAILED(Status)) 
	{
		DeleteSecurityContext(phContext);
		closesocket(hSocket);
		FreeCredentialsHandle(phCreds);
		return 1;
	}

	pbMessage = (PBYTE)OutBuffers[0].pvBuffer;
	cbMessage = OutBuffers[0].cbBuffer;

	//
	// Send the close notify message to the server.
	//

	if(pbMessage && cbMessage)
	{
		cbData = send(hSocket, (LPSTR)pbMessage, cbMessage, 0);
		if(cbData == SOCKET_ERROR || !cbData)
		{
			DeleteSecurityContext(phContext);
			closesocket(hSocket);
			FreeCredentialsHandle(phCreds);
			return 1;
		}

		// Free output buffer.
		FreeContextBuffer(pbMessage);
	}

	DeleteSecurityContext(phContext);
	closesocket(hSocket);
	FreeCredentialsHandle(phCreds);

	return 0;
}

SECURITY_STATUS CMessengerAuthentication::SecureClientHandshakeLoop(SOCKET hSocket,PCredHandle phCreds,CtxtHandle* phContext,BOOL fDoInitialRead,SecBuffer* pExtraData)
{
	SecBufferDesc	InBuffer;
	SecBuffer		InBuffers[2];
	SecBufferDesc	OutBuffer;
	SecBuffer		OutBuffers[1];
	DWORD			dwSSPIFlags;
	DWORD			dwSSPIOutFlags;
	TimeStamp		tsExpiry;
	SECURITY_STATUS	scRet;
	DWORD			cbData;

	PUCHAR			IoBuffer;
	DWORD			cbIoBuffer;
	BOOL			fDoRead;

	dwSSPIFlags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;

	//
	// Allocate data buffer.
	//

	IoBuffer = new UCHAR[IO_BUFFER_SIZE];
	if(!IoBuffer)
		return SEC_E_INTERNAL_ERROR;

	cbIoBuffer = 0;

	fDoRead = fDoInitialRead;

	// 
	// Loop until the handshake is finished or an error occurs.
	//

	scRet = SEC_I_CONTINUE_NEEDED;

	while(scRet == SEC_I_CONTINUE_NEEDED || scRet == SEC_E_INCOMPLETE_MESSAGE || scRet == SEC_I_INCOMPLETE_CREDENTIALS)
	{
		//
		// Read data from server.
		//

		if(!cbIoBuffer || scRet == SEC_E_INCOMPLETE_MESSAGE)
		{
			if(fDoRead)
			{
				cbData = recv(hSocket,(LPSTR)IoBuffer + cbIoBuffer,IO_BUFFER_SIZE - cbIoBuffer,0);
				if(cbData == SOCKET_ERROR)
				{
					scRet = SEC_E_INTERNAL_ERROR;
					break;
				}
				else if(!cbData)
				{
					scRet = SEC_E_INTERNAL_ERROR;
					break;
				}

				cbIoBuffer += cbData;
			}
			else
			{
				fDoRead = TRUE;
			}
		}

		//
		// Set up the input buffers. Buffer 0 is used to pass in data
		// received from the server. Schannel will consume some or all
		// of this. Leftover data (if any) will be placed in buffer 1 and
		// given a buffer type of SECBUFFER_EXTRA.
		//

		InBuffers[0].pvBuffer = IoBuffer;
		InBuffers[0].cbBuffer = cbIoBuffer;
		InBuffers[0].BufferType = SECBUFFER_TOKEN;

		InBuffers[1].pvBuffer = NULL;
		InBuffers[1].cbBuffer = 0;
		InBuffers[1].BufferType = SECBUFFER_EMPTY;

		InBuffer.cBuffers = 2;
		InBuffer.pBuffers = InBuffers;
		InBuffer.ulVersion = SECBUFFER_VERSION;

		//
		// Set up the output buffers. These are initialized to NULL
		// so as to make it less likely we'll attempt to free random
		// garbage later.
		//

		OutBuffers[0].pvBuffer = NULL;
		OutBuffers[0].BufferType= SECBUFFER_TOKEN;
		OutBuffers[0].cbBuffer = 0;

		OutBuffer.cBuffers = 1;
		OutBuffer.pBuffers = OutBuffers;
		OutBuffer.ulVersion = SECBUFFER_VERSION;

		//
		// Call InitializeSecurityContext.
		//

		scRet = InitializeSecurityContext(phCreds,phContext,NULL,dwSSPIFlags,0,SECURITY_NATIVE_DREP,&InBuffer,0,NULL,&OutBuffer,&dwSSPIOutFlags,&tsExpiry);

		//
		// If InitializeSecurityContext was successful (or if the error was 
		// one of the special extended ones), send the contends of the output
		// buffer to the server.
		//

		if(scRet == SEC_E_OK || scRet == SEC_I_CONTINUE_NEEDED || FAILED(scRet) && (dwSSPIOutFlags & ISC_RET_EXTENDED_ERROR))
		{
			if(OutBuffers[0].cbBuffer != 0 && OutBuffers[0].pvBuffer != NULL)
			{
				cbData = send(hSocket,(LPCSTR)OutBuffers[0].pvBuffer,OutBuffers[0].cbBuffer,0);
				if(cbData == SOCKET_ERROR || cbData == 0)
				{
					FreeContextBuffer(OutBuffers[0].pvBuffer);
					DeleteSecurityContext(phContext);
					return SEC_E_INTERNAL_ERROR;
				}

				// Free output buffer.
				FreeContextBuffer(OutBuffers[0].pvBuffer);
				OutBuffers[0].pvBuffer = NULL;
			}
		}


		//
		// If InitializeSecurityContext returned SEC_E_INCOMPLETE_MESSAGE,
		// then we need to read more data from the server and try again.
		//

		if(scRet == SEC_E_INCOMPLETE_MESSAGE)
			continue;

		//
		// If InitializeSecurityContext returned SEC_E_OK, then the 
		// handshake completed successfully.
		//

		if(scRet == SEC_E_OK)
		{
			//
			// If the "extra" buffer contains data, this is encrypted application
			// protocol layer stuff. It needs to be saved. The application layer
			// will later decrypt it with DecryptMessage.
			//

			if(pExtraData)
			{
				if(InBuffers[1].BufferType == SECBUFFER_EXTRA)
				{
					pExtraData->pvBuffer = new BYTE[InBuffers[1].cbBuffer];
					if(!pExtraData->pvBuffer)
						return SEC_E_INTERNAL_ERROR;

					MoveMemory(pExtraData->pvBuffer,IoBuffer + (cbIoBuffer - InBuffers[1].cbBuffer),InBuffers[1].cbBuffer);

					pExtraData->cbBuffer = InBuffers[1].cbBuffer;
					pExtraData->BufferType = SECBUFFER_TOKEN;
				
				}
				else
				{
					pExtraData->pvBuffer = NULL;
					pExtraData->cbBuffer = 0;
					pExtraData->BufferType = SECBUFFER_EMPTY;
				}
			}

			//
			// Bail out to quit
			//

			break;
		}

		//
		// Check for fatal error.
		//

		if(FAILED(scRet))
			break;

		//
		// If InitializeSecurityContext returned SEC_I_INCOMPLETE_CREDENTIALS,
		// then the server just requested client authentication. 
		//

		if(scRet == SEC_I_INCOMPLETE_CREDENTIALS)
		{
			//
			// Busted. The server has requested client authentication and
			// the credential we supplied didn't contain a client certificate.
			//

			//
			// This function will read the list of trusted certificate
			// authorities ("issuers") that was received from the server
			// and attempt to find a suitable client certificate that
			// was issued by one of these. If this function is successful, 
			// then we will connect using the new certificate. Otherwise,
			// we will attempt to connect anonymously (using our current
			// credentials).
			//

			//GetNewClientCredentials(phCreds, phContext);
			DebugBreak();

			// Go around again.
			fDoRead = FALSE;
			scRet = SEC_I_CONTINUE_NEEDED;
			continue;
		}


		//
		// Copy any leftover data from the "extra" buffer, and go around
		// again.
		//

		if(InBuffers[1].BufferType == SECBUFFER_EXTRA)
		{
			MoveMemory(IoBuffer,IoBuffer + (cbIoBuffer - InBuffers[1].cbBuffer),InBuffers[1].cbBuffer);

			cbIoBuffer = InBuffers[1].cbBuffer;
		}
		else
		{
			cbIoBuffer = 0;
		}
	}

	// Delete the security context in the case of a fatal error.
	if(FAILED(scRet))
		DeleteSecurityContext(phContext);

	delete[] IoBuffer;

	return scRet;
}

SECURITY_STATUS CMessengerAuthentication::SecurePerformClientHandshake(SOCKET hSocket,PCredHandle phCreds,LPCTSTR pszServerName,CtxtHandle* phContext,SecBuffer* pExtraData)
{
	SecBufferDesc	OutBuffer;
	SecBuffer		OutBuffers[1];
	DWORD			dwSSPIFlags;
	DWORD			dwSSPIOutFlags;
	TimeStamp		tsExpiry;
	SECURITY_STATUS	scRet;
	DWORD			cbData;

	dwSSPIFlags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;

	//
	//  Initiate a ClientHello message and generate a token.
	//

	OutBuffers[0].pvBuffer = NULL;
	OutBuffers[0].BufferType = SECBUFFER_TOKEN;
	OutBuffers[0].cbBuffer = 0;

	OutBuffer.cBuffers = 1;
	OutBuffer.pBuffers = OutBuffers;
	OutBuffer.ulVersion = SECBUFFER_VERSION;

	scRet = InitializeSecurityContext(phCreds,NULL,(SECURITY_PSTR)pszServerName,dwSSPIFlags,0,SECURITY_NATIVE_DREP,NULL,0,phContext,&OutBuffer,&dwSSPIOutFlags,&tsExpiry);
	if(scRet != SEC_I_CONTINUE_NEEDED)
		return scRet;

	// Send response to server if there is one.
	if(OutBuffers[0].cbBuffer && OutBuffers[0].pvBuffer)
	{
		cbData = send(hSocket,(LPCSTR)OutBuffers[0].pvBuffer,OutBuffers[0].cbBuffer,0);
		if(cbData == SOCKET_ERROR || !cbData)
		{
			FreeContextBuffer(OutBuffers[0].pvBuffer);
			DeleteSecurityContext(phContext);
			return SEC_E_INTERNAL_ERROR;
		}

		// Free output buffer.
		FreeContextBuffer(OutBuffers[0].pvBuffer);
		OutBuffers[0].pvBuffer = NULL;
	}

	return SecureClientHandshakeLoop(hSocket, phCreds, phContext, TRUE, pExtraData);
}

ULONG CMessengerAuthentication::SecureSendCommand(SOCKET hSocket, CredHandle hCreditentials, CtxtHandle hContext, LPCSTR command)
{
	SecPkgContext_StreamSizes	Sizes;
	SECURITY_STATUS	scRet;
	SecBufferDesc	Message;
	SecBuffer		Buffers[4];

	PBYTE	pbIoBuffer;
	DWORD	cbIoBufferLength;
	PBYTE	pbMessage;
	DWORD	cbMessage;

	DWORD	cbData;

	//
	// Read stream encryption properties.
	//

	scRet = QueryContextAttributes(&hContext,SECPKG_ATTR_STREAM_SIZES,&Sizes);
	if(scRet != SEC_E_OK)
		return 1;

	//
	// Allocate a working buffer. The plaintext sent to EncryptMessage
	// should never be more than 'Sizes.cbMaximumMessage', so a buffer 
	// size of this plus the header and trailer sizes should be safe enough.
	// 

	cbIoBufferLength = Sizes.cbHeader + Sizes.cbMaximumMessage +Sizes.cbTrailer;
	pbIoBuffer = new BYTE[cbIoBufferLength];
	if(!pbIoBuffer)
		return 1;

	//
	// Build an HTTP request to send to the server.
	//

	// Build the HTTP request offset into the data buffer by "header size"
	// bytes. This enables Schannel to perform the encryption in place,
	// which is a significant performance win.
	pbMessage = pbIoBuffer + Sizes.cbHeader;

	// Build HTTP request. Note that I'm assuming that this is less than
	// the maximum message size. If it weren't, it would have to be broken up.
	
	//sprintf(pbMessage,"%s\r\n",command);
	if((ULONG)lstrlenA(command) >= Sizes.cbMaximumMessage)
		DebugBreak();

	lstrcpyA((LPSTR)pbMessage,command);

	cbMessage = (DWORD)lstrlenA((LPCSTR)pbMessage);

	//
	// Encrypt the HTTP request.
	//

	Buffers[0].pvBuffer = pbIoBuffer;
	Buffers[0].cbBuffer = Sizes.cbHeader;
	Buffers[0].BufferType = SECBUFFER_STREAM_HEADER;

	Buffers[1].pvBuffer = pbMessage;
	Buffers[1].cbBuffer = cbMessage;
	Buffers[1].BufferType = SECBUFFER_DATA;

	Buffers[2].pvBuffer = pbMessage + cbMessage;
	Buffers[2].cbBuffer = Sizes.cbTrailer;
	Buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;

	Buffers[3].BufferType = SECBUFFER_EMPTY;

	Message.ulVersion = SECBUFFER_VERSION;
	Message.cBuffers = 4;
	Message.pBuffers = Buffers;

	scRet = EncryptMessage(&hContext, 0, &Message, 0);
	if(FAILED(scRet))
	{
		delete[] pbIoBuffer;
		return 1;
	}

	//
	// Send the encrypted data to the server.
	//

	cbData = send(hSocket,(LPCSTR)pbIoBuffer,Buffers[0].cbBuffer + Buffers[1].cbBuffer + Buffers[2].cbBuffer,0);
	if(cbData == SOCKET_ERROR)
	{
		delete[] pbIoBuffer;
		DeleteSecurityContext(&hContext);
		return -1;
	}

	if(cbData != Buffers[0].cbBuffer + Buffers[1].cbBuffer + Buffers[2].cbBuffer)
	{
		delete[] pbIoBuffer;
		DeleteSecurityContext(&hContext);
		return 1;
	}

	delete[] pbIoBuffer;
	return 0;
}

ULONG CMessengerAuthentication::SocketDataReady(SOCKET hSocket)
{
	struct fd_set fd;
	struct timeval time = {0,0};

	FD_ZERO(&fd);
	FD_SET(hSocket,&fd);

	switch(select(0,&fd,NULL,&fd,&time))
	{
	case 0: return 0;
	case SOCKET_ERROR: return -1;
	}

	return 1;
}

ULONG CMessengerAuthentication::SecureReceiveCommand(SOCKET hSocket, CredHandle hCreditentials, CtxtHandle hContext,LPBYTE buffer,ULONG size)
{
	LPBYTE data,pdata;
	ULONG dataSize = IO_BUFFER_SIZE;
	ULONG recvSize,psize,result,processed;
	SecBuffer buffers[4];
	SecBufferDesc message;
	
	//switch(SocketDataReady(hSocket))
	//{
	//case 0: return 0;
	//case -1: return -1;
	//}

	while(TRUE)
	{
		data = new BYTE[dataSize];
		if(!data)
			return -1;

		recvSize = recv(hSocket,(LPSTR)data, dataSize, MSG_PEEK);
		if(recvSize == SOCKET_ERROR)
		{
			delete[] data;
			return -1;
		}

		if(recvSize == 0)
		{
			delete[] data;
			return -1;
		}

		if(recvSize < dataSize)
			break;

		delete[] data;
		dataSize <<= 1;
	}

	processed = 0;
	pdata = data;
	psize = recvSize;

	while(TRUE)
	{
		buffers[0].pvBuffer = pdata;
		buffers[0].cbBuffer = psize;
		buffers[0].BufferType = SECBUFFER_DATA;

		buffers[1].BufferType = SECBUFFER_EMPTY;
		buffers[2].BufferType = SECBUFFER_EMPTY;
		buffers[3].BufferType = SECBUFFER_EMPTY;

		message.ulVersion = SECBUFFER_VERSION;
		message.cBuffers = 4;
		message.pBuffers = buffers;

		result = DecryptMessage(&hContext, &message, 0, NULL);

		if(result == SEC_E_INCOMPLETE_MESSAGE)
		{
			// Not enough data was received
			TRACE(TEXT("DecryptMessage : SEC_E_INCOMPLETE_MESSAGE\n"));
			delete[] data;
			return 0;
		}

		// Server signalled end of session
		if(result == SEC_I_CONTEXT_EXPIRED)
		{
			TRACE(TEXT("DecryptMessage : SEC_I_CONTEXT_EXPIRED\n"));
			delete[] data;
			return -1;
		}

		if(result == SEC_I_RENEGOTIATE)
			DebugBreak();

		if(result != SEC_E_OK)
			DebugBreak();

		if(size < processed + buffers[1].cbBuffer)
			DebugBreak();

		CopyMemory(buffer + processed,buffers[1].pvBuffer,buffers[1].cbBuffer);
		processed += buffers[1].cbBuffer;

		if(!buffers[3].BufferType)
			break;

		pdata = (LPBYTE)buffers[3].pvBuffer;
		psize = buffers[3].cbBuffer;
	}

	// Remove the data from the network buffer
	recv(hSocket,(LPSTR)data,recvSize,0);
	delete[] data;

	return processed;
}