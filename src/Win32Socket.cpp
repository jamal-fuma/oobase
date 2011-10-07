///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor
//
// This file is part of OOBase, the Omega Online Base library.
//
// OOBase is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OOBase is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with OOBase.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////////

#include "../include/OOBase/Memory.h"

#if defined(_WIN32)

#include "../include/OOBase/Singleton.h"
#include "Win32Socket.h"

#include <mswsock.h>

#if !defined(WSAID_ACCEPTEX)

typedef BOOL (PASCAL* LPFN_ACCEPTEX)(
    SOCKET sListenSocket,
    SOCKET sAcceptSocket,
    PVOID lpOutputBuffer,
    DWORD dwReceiveDataLength,
    DWORD dwLocalAddressLength,
    DWORD dwRemoteAddressLength,
    LPDWORD lpdwBytesReceived,
    LPOVERLAPPED lpOverlapped);

#define WSAID_ACCEPTEX {0xb5367df1,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}

#endif // !defined(WSAID_ACCEPTEX)

#if !defined(WSAID_GETACCEPTEXSOCKADDRS)

typedef VOID (PASCAL* LPFN_GETACCEPTEXSOCKADDRS)(
    PVOID lpOutputBuffer,
    DWORD dwReceiveDataLength,
    DWORD dwLocalAddressLength,
    DWORD dwRemoteAddressLength,
    sockaddr **LocalSockaddr,
    LPINT LocalSockaddrLength,
    sockaddr **RemoteSockaddr,
    LPINT RemoteSockaddrLength
    );

#define WSAID_GETACCEPTEXSOCKADDRS {0xb5367df2,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}

#endif // !defined(WSAID_GETACCEPTEXSOCKADDRS)

#if !defined(WSAID_CONNECTEX)

typedef BOOL (PASCAL * LPFN_CONNECTEX)(
    SOCKET s,
    const sockaddr *name,
    int namelen,
    PVOID lpSendBuffer,
    DWORD dwSendDataLength,
    LPDWORD lpdwBytesSent,
    LPOVERLAPPED lpOverlapped
    );

#define WSAID_CONNECTEX {0x25a207b9,0xddf3,0x4660,{0x8e,0xe9,0x76,0xe5,0x8c,0x74,0x06,0x3e}}

#if !defined(SO_UPDATE_CONNECT_CONTEXT)
#define SO_UPDATE_CONNECT_CONTEXT   0x7010
#endif

#endif // !defined(WSAID_CONNECTEX)

namespace
{
	void DoWSACleanup(void*)
	{
		WSACleanup();
	}
}

namespace OOBase
{
	// The discrimination type for singleton scoping for this module
	struct Module
	{
		int unused;
	};
}

void OOBase::Win32::WSAStartup()
{
	// Start Winsock
	WSADATA wsa;
	int err = WSAStartup(MAKEWORD(2,2),&wsa);
	if (err != 0)
		OOBase_CallCriticalFailure(WSAGetLastError());
	
	if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2)
	{
		WSACleanup();
		OOBase_CallCriticalFailure("Very old Winsock dll");
	}

	OOBase::DLLDestructor<OOBase::Module>::add_destructor(&DoWSACleanup,NULL);
}

BOOL OOBase::Win32::WSAAcceptEx(SOCKET sListenSocket, SOCKET sAcceptSocket, void* lpOutputBuffer, DWORD dwReceiveDataLength, DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, LPDWORD lpdwBytesReceived, LPOVERLAPPED lpOverlapped)
{
	//----------------------------------------
	// Load the AcceptEx function into memory using WSAIoctl.
	// The WSAIoctl function is an extension of the ioctlsocket()
	// function that can use overlapped I/O. The function's 3rd
	// through 6th parameters are input and output buffers where
	// we pass the pointer to our AcceptEx function. This is used
	// so that we can call the AcceptEx function directly, rather
	// than refer to the Mswsock.lib library.
	static const GUID guid_AcceptEx = WSAID_ACCEPTEX;

	LPFN_ACCEPTEX lpfnAcceptEx = NULL;
	DWORD dwBytes = 0;
	if (WSAIoctl(sListenSocket, 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		(void*)&guid_AcceptEx, 
		sizeof(guid_AcceptEx),
		&lpfnAcceptEx, 
		sizeof(lpfnAcceptEx), 
		&dwBytes, 
		NULL, 
		NULL) != 0)
	{
		OOBase_CallCriticalFailure("Failed to load address of AcceptEx");
	}

	return (*lpfnAcceptEx)(sListenSocket,sAcceptSocket,lpOutputBuffer,dwReceiveDataLength,dwLocalAddressLength,dwRemoteAddressLength,lpdwBytesReceived,lpOverlapped);
}

void OOBase::Win32::WSAGetAcceptExSockAddrs(SOCKET sListenSocket, void* lpOutputBuffer, DWORD dwReceiveDataLength, DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, sockaddr **LocalSockaddr, int* LocalSockaddrLength, sockaddr **RemoteSockaddr, int* RemoteSockaddrLength)
{
	static const GUID guid_GetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;

	LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockAddrs = NULL;
	DWORD dwBytes = 0;
	if (WSAIoctl(sListenSocket, 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		(void*)&guid_GetAcceptExSockAddrs, 
		sizeof(guid_GetAcceptExSockAddrs),
		&lpfnGetAcceptExSockAddrs, 
		sizeof(lpfnGetAcceptExSockAddrs), 
		&dwBytes, 
		NULL, 
		NULL) != 0)
	{
		OOBase_CallCriticalFailure("Failed to load address of GetAcceptExSockAddrs");
	}

	return (*lpfnGetAcceptExSockAddrs)(lpOutputBuffer,dwReceiveDataLength,dwLocalAddressLength,dwRemoteAddressLength,LocalSockaddr,LocalSockaddrLength,RemoteSockaddr,RemoteSockaddrLength);
}

SOCKET OOBase::Win32::create_socket(int family, int socktype, int protocol, int& err)
{
	Win32::WSAStartup();

	err = 0;

	SOCKET sock = WSASocket(family,socktype,protocol,NULL,0,WSA_FLAG_OVERLAPPED);
	if (sock == INVALID_SOCKET)
		err = WSAGetLastError();

	return sock;
}

int OOBase::Win32::connect(SOCKET sock, const sockaddr* addr, socklen_t addrlen, const OOBase::timeval_t* timeout)
{
	static const GUID guid_ConnectEx = WSAID_CONNECTEX;

	LPFN_CONNECTEX lpfnConnectEx = NULL;

	if (timeout)
	{
		DWORD dwBytes = 0;
		WSAIoctl(sock, 
			SIO_GET_EXTENSION_FUNCTION_POINTER, 
			(void*)&guid_ConnectEx, 
			sizeof(guid_ConnectEx),
			&lpfnConnectEx, 
			sizeof(lpfnConnectEx), 
			&dwBytes, 
			NULL, 
			NULL);
	}

	if (lpfnConnectEx && timeout)
	{
		WSAOVERLAPPED ov = {0};
		ov.hEvent = CreateEventW(NULL,TRUE,FALSE,NULL);
		if (!ov.hEvent)
			return GetLastError();
		
		if (!(*lpfnConnectEx)(sock,addr,addrlen,NULL,0,NULL,&ov))
		{
			DWORD dwErr = WSAGetLastError();
			if (dwErr != ERROR_IO_PENDING)
				return dwErr;

			DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout->msec());
			if (dwWait == WAIT_TIMEOUT)
			{
				CancelIo((HANDLE)sock);
				return WSAETIMEDOUT;
			}
			else if (dwWait != WAIT_OBJECT_0)
			{
				dwErr = GetLastError();
				CancelIo((HANDLE)sock);
				return dwErr;
			}
		}

		setsockopt(sock,SOL_SOCKET,SO_UPDATE_CONNECT_CONTEXT,NULL,0);

		return 0;
	}
	else
	{
		// Do the connect
		if (connect(sock,addr,static_cast<int>(addrlen)) != SOCKET_ERROR)
			return 0;
		else
			return WSAGetLastError();
	}
}

namespace
{
	class WinSocket : public OOBase::Socket
	{
	public:
		WinSocket(SOCKET sock);
		virtual ~WinSocket();

		size_t recv(void* buf, size_t len, bool bAll, int& err, const OOBase::timeval_t* timeout = NULL);
		int recv_v(OOBase::Buffer* buffers[], size_t count, const OOBase::timeval_t* timeout = NULL);
		
		size_t send(const void* buf, size_t len, int& err, const OOBase::timeval_t* timeout = NULL);
		int send_v(OOBase::Buffer* buffers[], size_t count, const OOBase::timeval_t* timeout = NULL);

		void close();
					
	private:
		SOCKET                     m_socket;
		OOBase::Mutex              m_recv_lock;  // These are mutexes to enforce ordering
		OOBase::Mutex              m_send_lock;
		OOBase::Win32::SmartHandle m_recv_event;
		OOBase::Win32::SmartHandle m_send_event;

		DWORD send_i(WSABUF* wsabuf, DWORD count, int& err, const OOBase::timeval_t* timeout);
		DWORD recv_i(WSABUF* wsabuf, DWORD count, bool bAll, int& err, const OOBase::timeval_t* timeout);
	};

	SOCKET connect_i(const char* address, const char* port, int& err, const OOBase::timeval_t* timeout)
	{
		// Start a countdown
		OOBase::timeval_t timeout2 = (timeout ? *timeout : OOBase::timeval_t::MaxTime);
		OOBase::Countdown countdown(&timeout2);

		// Resolve the passed in addresses...
		addrinfo hints = {0};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		addrinfo* pResults = NULL;
		if (getaddrinfo(address,port,&hints,&pResults) != 0)
		{
			err = WSAGetLastError();
			return INVALID_SOCKET;
		}
		
		SOCKET sock = INVALID_SOCKET;

		// Took too long to resolve...
		countdown.update();
		if (timeout2 == OOBase::timeval_t::Zero)
			err = WSAETIMEDOUT;
		else
		{
			// Loop trying to connect on each address until one succeeds
			for (addrinfo* pAddr = pResults; pAddr != NULL; pAddr = pAddr->ai_next)
			{
				// Clear error
				err = 0;
				sock = OOBase::Win32::create_socket(pAddr->ai_family,pAddr->ai_socktype,pAddr->ai_protocol,err);
				if (sock == INVALID_SOCKET)
					break;
				else
				{
					if ((err = OOBase::Win32::connect(sock,pAddr->ai_addr,static_cast<socklen_t>(pAddr->ai_addrlen),timeout ? &timeout2 : NULL)) != 0)
						closesocket(sock);
					else
						break;
				}

				countdown.update();
				if (timeout2 == OOBase::timeval_t::Zero)
				{
					err = WSAETIMEDOUT;
					break;
				}
			}
		}

		// Done with address info
		freeaddrinfo(pResults);

		return sock;
	}
}

WinSocket::WinSocket(SOCKET sock) :
		m_socket(sock)
{
	OOBase::Win32::WSAStartup();
}

WinSocket::~WinSocket()
{
	closesocket(m_socket);
}

size_t WinSocket::send(const void* buf, size_t len, int& err, const OOBase::timeval_t* timeout)
{
	err = 0;
	if (len == 0)
		return 0;

	if (len > 0xFFFFFFFF)
	{
		err = E2BIG;
		return 0;
	}

	WSABUF buf;
	buf.buf = const_cast<char*>(static_cast<const char*>(buf));
	buf.len = static_cast<ULONG>(len);

	return send_i(&buf,1,err,timeout);
}

int WinSocket::send_v(OOBase::Buffer* buffers[], size_t count, const OOBase::timeval_t* timeout)
{
	if (count == 0)
		return 0;

	WSABUF static_bufs[4];
	WSABUF* bufs = static_bufs;
	OOBase::SmartPtr<WSABUF,OOBase::LocalAllocator> ptrBufs;

	size_t total_len = 0;
	DWORD actual_count = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->length();

		total_len += len;
		if (total_len > 0xFFFFFFFF)
			return E2BIG;

		if (len > 0)
		{
			if (actual_count == 0xFFFFFFFF)
				return E2BIG;

			++actual_count;
		}
	}

	if (actual_count > sizeof(static_bufs)/sizeof(static_bufs[0]))
	{
		ptrBufs = static_cast<WSABUF*>(OOBase::LocalAllocate(actual_count * sizeof(WSABUF)));
		if (!ptrBufs)
			return ERROR_OUTOFMEMORY;

		bufs = ptrBufs;
	}

	DWORD j = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->length();
		if (len > 0)
		{
			bufs[j].len = static_cast<ULONG>(len);
			bufs[j].buf = const_cast<char*>(static_cast<const char*>(buffers[i]->rd_ptr()));
			++j;
		}
	}

	int err = 0;
	DWORD dwWritten = send_i(bufs,actual_count,err,timeout);

	// Update buffers...
	for (size_t first_buffer = 0;dwWritten;)
	{
		if (dwWritten >= bufs->len)
		{
			buffers[first_buffer]->rd_ptr(bufs->len);
			++first_buffer;
			dwWritten -= bufs->len;
			++bufs;
			if (--actual_count == 0)
				break;
		}
		else
		{
			buffers[first_buffer]->rd_ptr(dwWritten);
			bufs->len -= dwWritten;
			bufs->buf += dwWritten;
			dwWritten = 0;
		}
	}

	return err;
}

DWORD WinSocket::send_i(WSABUF* wsabuf, DWORD count, int& err, const OOBase::timeval_t* timeout)
{
	WSAOVERLAPPED ov = {0};
	OOBase::Guard<OOBase::Mutex> guard(m_send_lock,false);

	if (timeout)
	{
		if (!guard.acquire(*timeout))
		{
			err = WSAETIMEDOUT;
			return 0;
		}

		if (!m_send_event.is_valid())
		{
			m_send_event = CreateEventW(NULL,TRUE,FALSE,NULL);
			if (!m_send_event.is_valid())
			{
				err = GetLastError();
				return 0;
			}
		}

		ov.hEvent = m_send_event;
	}

	DWORD dwWritten = 0;
	if (!timeout)
	{
		if (WSASend(m_socket,wsabuf,static_cast<DWORD>(count),&dwWritten,0,NULL,NULL) != 0)
			err = WSAGetLastError();
	}
	else
	{
		if (WSASend(m_socket,wsabuf,static_cast<DWORD>(count),&dwWritten,0,&ov,NULL) != 0)
		{
			err = WSAGetLastError();
			if (err == WSA_IO_PENDING)
			{
				err = 0;
				DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout->msec());
				if (dwWait == WAIT_TIMEOUT)
				{
					CancelIo(reinterpret_cast<HANDLE>(m_socket));
					err = WAIT_TIMEOUT;
				}
				else if (dwWait != WAIT_OBJECT_0)
				{
					err = GetLastError();
					CancelIo(reinterpret_cast<HANDLE>(m_socket));
				}

				DWORD dwFlags = 0;
				if (!WSAGetOverlappedResult(m_socket,&ov,&dwWritten,TRUE,&dwFlags))
				{
					int dwErr = WSAGetLastError();
					if (err == 0 && dwErr != ERROR_OPERATION_ABORTED)
						err = dwErr;
				}
			}
		}
	}

	return dwWritten;
}

size_t WinSocket::recv(void* buf, size_t len, bool bAll, int& err, const OOBase::timeval_t* timeout)
{
	err = 0;
	if (len == 0)
		return 0;

	if (len > 0xFFFFFFFF)
	{
		err = E2BIG;
		return 0;
	}

	WSABUF buf;
	buf.buf = static_cast<char*>(buf);
	buf.len = static_cast<ULONG>(len);

	return recv_i(&buf,1,bAll,err,timeout);
}

int WinSocket::recv_v(OOBase::Buffer* buffers[], size_t count, const OOBase::timeval_t* timeout)
{
	if (count == 0)
		return 0;

	WSABUF static_bufs[4];
	WSABUF* bufs = static_bufs;
	OOBase::SmartPtr<WSABUF,OOBase::LocalAllocator> ptrBufs;

	size_t total_len = 0;
	DWORD actual_count = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->space();

		total_len += len;
		if (total_len > 0xFFFFFFFF)
			return E2BIG;

		if (len > 0)
		{
			if (actual_count == 0xFFFFFFFF)
				return E2BIG;

			++actual_count;
		}
	}

	if (actual_count > sizeof(static_bufs)/sizeof(static_bufs[0]))
	{
		ptrBufs = static_cast<WSABUF*>(OOBase::LocalAllocate(actual_count * sizeof(WSABUF)));
		if (!ptrBufs)
			return ERROR_OUTOFMEMORY;

		bufs = ptrBufs;
	}

	DWORD j = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->space();
		if (len > 0)
		{
			bufs[j].len = static_cast<ULONG>(len);
			bufs[j].buf = const_cast<char*>(buffers[i]->wr_ptr());
			++j;
		}
	}

	int err = 0;
	DWORD dwRead = recv_i(bufs,actual_count,bAll,err,timeout);

	// Update buffers...
	for (size_t first_buffer = 0;dwRead;)
	{
		if (dwRead >= bufs->len)
		{
			buffers[first_buffer]->wr_ptr(bufs->len);
			++first_buffer;
			dwRead -= bufs->len;
			++bufs;
			if (--actual_count == 0)
				break;
		}
		else
		{
			buffers[first_buffer]->wr_ptr(dwRead);
			bufs->len -= dwRead;
			bufs->buf += dwRead;
			dwRead = 0;
		}
	}

	return err;
}

DWORD WinSocket::recv_i(WSABUF* wsabuf, DWORD count, bool bAll, int& err, const OOBase::timeval_t* timeout)
{
	WSAOVERLAPPED ov = {0};

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock,false);

	if (timeout)
	{
		if (!guard.acquire(*timeout))
		{
			err = WSAETIMEDOUT;
			return 0;
		}

		if (!m_recv_event.is_valid())
		{
			m_recv_event = CreateEventW(NULL,TRUE,FALSE,NULL);
			if (!m_recv_event.is_valid())
			{
				err = GetLastError();
				return 0;
			}
		}

		ov.hEvent = m_recv_event;
	}

	DWORD dwFlags = (bAll ? MSG_WAITALL : 0);
	DWORD dwRead = 0;
	if (!timeout)
	{
		if (WSARecv(m_socket,wsabuf,static_cast<DWORD>(count),&dwRead,&dwFlags,NULL,NULL) != 0)
			err = WSAGetLastError();
	}
	else
	{
		if (WSARecv(m_socket,wsabuf,static_cast<DWORD>(count),&dwRead,&dwFlags,&ov,NULL) != 0)
		{
			err = WSAGetLastError();
			if (err == WSA_IO_PENDING)
			{
				err = 0;
				DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout->msec());
				if (dwWait == WAIT_TIMEOUT)
				{
					CancelIo(reinterpret_cast<HANDLE>(m_socket));
					err = WAIT_TIMEOUT;
				}
				else if (dwWait != WAIT_OBJECT_0)
				{
					err = GetLastError();
					CancelIo(reinterpret_cast<HANDLE>(m_socket));
				}

				if (!WSAGetOverlappedResult(m_socket,&ov,&dwRead,TRUE,&dwFlags))
				{
					int dwErr = WSAGetLastError();
					if (err == 0 && dwErr != ERROR_OPERATION_ABORTED)
						err = dwErr;
				}
			}
		}
	}

	return dwRead;
}

void WinSocket::close()
{
	// This may not cancel any outstanding recvs
	void* CHECK_ME;

	::shutdown(m_socket,SD_SEND);
}

OOBase::Socket* OOBase::Socket::connect(const char* address, const char* port, int& err, const timeval_t* timeout)
{
	// Ensure we have winsock loaded
	Win32::WSAStartup();

	SOCKET sock = connect_i(address,port,err,timeout);
	if (sock == INVALID_SOCKET)
		return NULL;

	OOBase::Socket* pSocket = new (std::nothrow) WinSocket(sock);
	if (!pSocket)
	{
		err = ERROR_OUTOFMEMORY;
		closesocket(sock);
	}

	return pSocket;
}

#endif // _WIN32
