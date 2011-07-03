///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2011 Rick Taylor
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

#include "../include/OOBase/GlobalNew.h"

#if defined(_WIN32)

#include "../include/OOBase/String.h"
#include "../include/OOBase/Socket.h"
#include "../include/OOBase/Win32.h"

namespace
{
	struct TxDirection
	{
		enum
		{
			Recv = 1,
			Send = 2
		};
	};

	class Pipe : public OOBase::Socket
	{
	public:
		Pipe(HANDLE handle);
		virtual ~Pipe();

		size_t recv(void* buf, size_t len, bool bAll, int& err, const OOBase::timeval_t* timeout);
		size_t recv_v(OOBase::Buffer* buffers[], size_t count, int& err, const OOBase::timeval_t* timeout);
		
		size_t send(const void* buf, size_t len, int& err, const OOBase::timeval_t* timeout);
		size_t send_v(OOBase::Buffer* buffers[], size_t count, int& err, const OOBase::timeval_t* timeout);
		
		void shutdown(bool bSend, bool bRecv);
					
	private:
		OOBase::Win32::SmartHandle m_handle;
		OOBase::Mutex              m_recv_lock;  // These are mutexes to enforce ordering
		OOBase::Mutex              m_send_lock;
		OOBase::Win32::SmartHandle m_recv_event;
		OOBase::Win32::SmartHandle m_send_event;
		bool                       m_send_allowed;
		bool                       m_recv_allowed;

		size_t recv_i(void* buf, size_t len, bool bAll, int& err, const OOBase::timeval_t* timeout);
		size_t send_i(const void* buf, size_t len, int& err, const OOBase::timeval_t* timeout);
	};
}

Pipe::Pipe(HANDLE handle) :
		m_handle(handle),
		m_send_allowed(true),
		m_recv_allowed(true)
{
}

Pipe::~Pipe()
{
}

size_t Pipe::send(const void* buf, size_t len, int& err, const OOBase::timeval_t* timeout)
{
	OOBase::Guard<OOBase::Mutex> guard(m_send_lock);

	return send_i(buf,len,err,timeout);
}

size_t Pipe::send_v(OOBase::Buffer* buffers[], size_t count, int& err, const OOBase::timeval_t* timeout)
{
	if (count == 0)
		return 0;

	void* TODO;

	return 0;
}

size_t Pipe::send_i(const void* buf, size_t len, int& err, const OOBase::timeval_t* timeout)
{
	if (!m_send_allowed)
	{
		err = WSAESHUTDOWN;
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

	OVERLAPPED ov = {0};
	ov.hEvent = m_send_event;
		
	const char* cbuf = static_cast<const char*>(buf);
	size_t total = len;
	while (total > 0)
	{
		DWORD dwWritten = 0;
		if (!WriteFile(m_handle,cbuf,static_cast<DWORD>(total),&dwWritten,&ov))
		{
			err = GetLastError();
			if (err != ERROR_OPERATION_ABORTED)
			{
				if (err != ERROR_IO_PENDING)
					return (len - total);

				err = 0;
				if (timeout)
				{
					DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout->msec());
					if (dwWait == WAIT_TIMEOUT)
					{
						CancelIo(m_handle);
						err = WAIT_TIMEOUT;
					}
					else if (dwWait != WAIT_OBJECT_0)
					{
						err = GetLastError();
						CancelIo(m_handle);
					}
				}
			
				if (!GetOverlappedResult(m_handle,&ov,&dwWritten,TRUE))
				{
					int dwErr = GetLastError();
					if (err == 0 && dwErr != ERROR_OPERATION_ABORTED)
						err = dwErr;

					if (err != 0)
						return (len - total);
				}
			}
		}
		
		cbuf += dwWritten;
		total -= dwWritten;
	}

	err = 0;
	return len;
}

size_t Pipe::recv(void* buf, size_t len, bool bAll, int& err, const OOBase::timeval_t* timeout)
{
	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock);

	return recv_i(buf,len,bAll,err,timeout);
}

size_t Pipe::recv_v(OOBase::Buffer* buffers[], size_t count, int& err, const OOBase::timeval_t* timeout)
{
	err = 0;

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock);

	size_t total = 0;
	for (size_t i=0;i<count && err == 0 ;++i)
	{
		size_t space = buffers[i]->space();
		size_t len = recv_i(buffers[i]->wr_ptr(),space,true,err,timeout);

		buffers[i]->wr_ptr(len);

		total += len;
	}
	
	return total;
}

size_t Pipe::recv_i(void* buf, size_t len, bool bAll, int& err, const OOBase::timeval_t* timeout)
{
	if (!m_recv_allowed)
	{
		err = WSAESHUTDOWN;
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

	OVERLAPPED ov = {0};
	ov.hEvent = m_recv_event;
		
	char* cbuf = static_cast<char*>(buf);
	size_t total = len;
	while (total > 0)
	{
		DWORD dwRead = 0;
		if (!ReadFile(m_handle,cbuf,static_cast<DWORD>(total),&dwRead,&ov))
		{
			err = GetLastError();
			if (err == ERROR_MORE_DATA)
				dwRead = static_cast<DWORD>(total);
			else if (err != ERROR_OPERATION_ABORTED)
			{
				if (err != ERROR_IO_PENDING)
					return (len - total);

				err = 0;
				if (timeout)
				{
					DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout->msec());
					if (dwWait == WAIT_TIMEOUT)
					{
						CancelIo(m_handle);
						err = WAIT_TIMEOUT;
					}
					else if (dwWait != WAIT_OBJECT_0)
					{
						err = GetLastError();
						CancelIo(m_handle);
					}
				}
					
				if (!GetOverlappedResult(m_handle,&ov,&dwRead,TRUE))
				{
					int dwErr = GetLastError();
					if (err == 0 && dwErr != ERROR_OPERATION_ABORTED)
						err = dwErr;

					if (err != 0)
						return (len - total);
				}
			}
		}

		cbuf += dwRead;
		total -= dwRead;

		if (!bAll && dwRead > 0)
			break;
	}

	err = 0;
	return (len - total);
}

void Pipe::shutdown(bool bSend, bool bRecv)
{
	if (bRecv)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_recv_lock);

		m_recv_allowed = false;
	}
	
	if (bSend)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_send_lock);

		m_send_allowed = false;
	}
}

OOBase::Socket* OOBase::Socket::connect_local(const char* path, int& err, const timeval_t* timeout)
{
	OOBase::LocalString pipe_name;
	err = pipe_name.assign("\\\\.\\pipe\\");
	if (err == 0)
		err = pipe_name.append(path);
	if (err != 0)
		return NULL;

	timeval_t timeout2 = (timeout ? *timeout : timeval_t::MaxTime);
	Countdown countdown(&timeout2);

	Win32::SmartHandle hPipe;
	while (timeout2 != timeval_t::Zero)
	{
		hPipe = CreateFileA(pipe_name.c_str(),
							PIPE_ACCESS_DUPLEX,
							0,
							NULL,
							OPEN_EXISTING,
							FILE_FLAG_OVERLAPPED,
							NULL);

		if (hPipe != INVALID_HANDLE_VALUE)
			break;

		DWORD dwErr = GetLastError();
		if (dwErr != ERROR_PIPE_BUSY)
		{
			err = dwErr;
			return 0;
		}

		DWORD dwWait = NMPWAIT_USE_DEFAULT_WAIT;
		if (timeout)
		{
			countdown.update();

			dwWait = timeout2.msec();
		}

		if (!WaitNamedPipeA(pipe_name.c_str(),dwWait))
		{
			err = GetLastError();
			if (err == ERROR_SEM_TIMEOUT)
				err = WSAETIMEDOUT;

			return 0;
		}
	}

	OOBase::Socket* pPipe = new (std::nothrow) Pipe(hPipe);
	if (!pPipe)
		err = ERROR_OUTOFMEMORY;
	else
		hPipe.detach();

	return pPipe;
}

#endif // _WIN32
