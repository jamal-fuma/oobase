///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor
//
// This file is part of OOSvrBase, the Omega Online Base library.
//
// OOSvrBase is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OOSvrBase is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with OOSvrBase.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////////

#include "../include/OOSvrBase/Proactor.h"

#include "../include/OOSvrBase/internal/ProactorImpl.h"

#if defined(HAVE_EV_H)
#include "../include/OOSvrBase/internal/ProactorEv.h"
#elif defined(_WIN32)
#include "../include/OOSvrBase/internal/ProactorWin32.h"
#else
#error No suitable proactor implementation!
#endif

#if defined(_WIN32)
#define ETIMEDOUT WSAETIMEDOUT
#define ENOTCONN  WSAENOTCONN
#else
#define ERROR_OUTOFMEMORY ENOMEM
#endif

OOSvrBase::Proactor::Proactor() :
		m_impl(0)
{
#if defined(HAVE_EV_H)
	OOBASE_NEW(m_impl,Ev::ProactorImpl());
#elif defined(_WIN32)
	OOBASE_NEW(m_impl,Win32::ProactorImpl());
#endif

	if (!m_impl)
		OOBase_OutOfMemory();
}

OOSvrBase::Proactor::Proactor(bool) :
		m_impl(0)
{
}

OOSvrBase::Proactor::~Proactor()
{
	delete m_impl;
}

OOBase::Socket* OOSvrBase::Proactor::accept_local(Acceptor<AsyncLocalSocket>* handler, const std::string& path, int* perr, SECURITY_ATTRIBUTES* psa)
{
	return m_impl->accept_local(handler,path,perr,psa);
}

OOBase::Socket* OOSvrBase::Proactor::accept_remote(Acceptor<AsyncSocket>* handler, const std::string& address, const std::string& port, int* perr)
{
	return m_impl->accept_remote(handler,address,port,perr);
}

OOSvrBase::AsyncSocket* OOSvrBase::Proactor::attach_socket(OOBase::Socket::socket_t sock, int* perr)
{
	return m_impl->attach_socket(sock,perr);
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::Proactor::attach_local_socket(OOBase::Socket::socket_t sock, int* perr)
{
	return m_impl->attach_local_socket(sock,perr);
}

OOSvrBase::detail::AsyncQueued::AsyncQueued(bool sender, AsyncIOHelper* helper, AsyncHandler* handler) :
		m_sender(sender),
		m_helper(helper),
		m_handler(handler),
		m_closed(false)
{
}

OOSvrBase::detail::AsyncQueued::~AsyncQueued()
{
	if (m_sender)
		delete m_helper;
}

void OOSvrBase::detail::AsyncQueued::dispose()
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

	// Issue the shutdown
	m_helper->shutdown(m_sender,!m_sender);

	if (!m_sender)
	{
		// Block waiting for all recv's to finish...
		while (!m_ops.empty() || !m_closed)
		{
			m_condition.wait(m_lock);
		}
	}
}

int OOSvrBase::detail::AsyncQueued::async_op(OOBase::Buffer* buffer, size_t len)
{
	assert(buffer);

	if (len > 0)
	{
		int err = buffer->space(len);
		if (err != 0)
			return err;
	}

	return async_op(buffer,len,0);
}

int OOSvrBase::detail::AsyncQueued::async_op(OOBase::Buffer* buffer, size_t len, BlockingInfo* param)
{
	// Build a new AsyncOp
	AsyncIOHelper::AsyncOp* op = 0;
	OOBASE_NEW(op,AsyncIOHelper::AsyncOp);
	if (!op)
		return ERROR_OUTOFMEMORY;
	
	op->buffer = buffer->duplicate();
	op->len = len;
	op->param = param;

	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

	if (m_closed)
	{
		op->buffer->release();
		delete op;
		return ENOTCONN;
	}

	// See if we have an op in progress
	bool bIssueNow = m_ops.empty();
	
	// Insert into queue
	try
	{
		m_ops.push(op);
	} 
	catch (std::exception& e)
	{
		OOBase_CallCriticalFailure(e.what());
	}

	if (bIssueNow)
		issue_next(guard);

	return 0;
}

void OOSvrBase::detail::AsyncQueued::issue_next(OOBase::Guard<OOBase::Condition::Mutex>& guard)
{
	// Lock is assumed to be held...

	if (!m_ops.empty())
	{
		AsyncIOHelper::AsyncOp* op = 0;
		try
		{
			op = m_ops.front();
			m_ops.pop();
		} 
		catch (std::exception& e)
		{
			OOBase_CallCriticalFailure(e.what());
		}

		// Release the lock, because errors will synchrnously call the handler...
		guard.release();

		// If we have an op... perform it
		if (m_sender)
			return m_helper->send(op);
		else
			return m_helper->recv(op);
	}
	
	if (m_closed)
	{
		// Now notify handler that we have closed and finished all ops
		m_handler->on_closed();
	}

	// And let waiter know we have an empty queue...
	guard.release();
	m_condition.signal();
}

bool OOSvrBase::detail::AsyncQueued::notify_async(AsyncIOHelper::AsyncOp* op, int err)
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock,false);

	// Check close status...
	bool bClose = m_handler->is_close(err);

	bool bRelease = true;
	if (op->param)
	{
		// Acquire the lock... because block_info may be manipulated...
		guard.acquire();

		BlockingInfo* block_info = static_cast<BlockingInfo*>(op->param);

		// Block recv complete
		if (block_info->cancelled)
		{
			// But was cancelled...

			// It's our responsibility to delete param, as the waiter has gone...
			delete block_info;
		}
		else
		{
			// Signal completion
			block_info->err = err;
			block_info->ev.set();	
		}

		// Done with our copy of the op
		op->buffer->release();
		delete op;

		// This is a sync op, caller need not release() us
		bRelease = false;
	}
	else
	{
		// Call handler
		if (m_sender)
			m_handler->on_sent(op->buffer,err);
		else
		{
			if (bClose)
			{
				// Report if we have recv'd something
				if (op->buffer->length())
					m_handler->on_recv(op->buffer,0);
			}
			else
				m_handler->on_recv(op->buffer,err);
		}
		
		// Done with our copy of the op
		op->buffer->release();
		delete op;

		// Acquire lock... we need it for issue_next
		guard.acquire();
	}

	if (bClose)
		m_closed = true;

	issue_next(guard);

	return bRelease;
}

int OOSvrBase::detail::AsyncQueued::sync_op(OOBase::Buffer* buffer, size_t len, const OOBase::timeval_t* timeout)
{
	BlockingInfo* block_info = 0;
	OOBASE_NEW(block_info,BlockingInfo);
	if (!block_info)
		return ERROR_OUTOFMEMORY;

	block_info->cancelled = false;
	block_info->err = 0;
	
	int err = async_op(buffer,len,block_info);
	if (err)
	{
		delete block_info;
	}
	else
	{
		// Wait for the event
		if (!block_info->ev.wait(timeout))
		{
			OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

			// Just check we haven't just been signalled
			if (!block_info->ev.is_set())
			{
				// Clear the event pointer so it is just dropped later...
				block_info->cancelled = true;

				return ETIMEDOUT;
			}
		}
		
		// Copy the error code...
		err = block_info->err;

		// Done with param...
		delete block_info;
	}

	return err;
}

OOSvrBase::detail::AsyncSocketImpl::AsyncSocketImpl(AsyncIOHelper* helper, AsyncHandler* handler) :
		m_receiver(false,helper,this),
		m_sender(true,helper,this),
		m_refcount(1),
		m_handler(handler)
{
	helper->bind_handler(this);
}

OOSvrBase::detail::AsyncSocketImpl::~AsyncSocketImpl()
{
}

void OOSvrBase::detail::AsyncSocketImpl::dispose()
{
	// Dispose both sender and receiver
	m_receiver.dispose();
	m_sender.dispose();

	release();
}

void OOSvrBase::detail::AsyncSocketImpl::addref()
{
 	++m_refcount;
}

void OOSvrBase::detail::AsyncSocketImpl::release()
{
	if (--m_refcount == 0)
		delete this;
}

int OOSvrBase::detail::AsyncSocketImpl::async_recv(OOBase::Buffer* buffer, size_t len)
{
	addref();
	int err = m_receiver.async_op(buffer,len);
	if (err != 0)
		release();

	return err;
}

void OOSvrBase::detail::AsyncSocketImpl::on_recv(AsyncIOHelper::AsyncOp* recv_op, int err)
{
	if (m_receiver.notify_async(recv_op,err))
		release();
}

int OOSvrBase::detail::AsyncSocketImpl::recv(OOBase::Buffer* buffer, size_t len, const OOBase::timeval_t* timeout)
{
	return m_receiver.sync_op(buffer,len,timeout);
}
		
int OOSvrBase::detail::AsyncSocketImpl::async_send(OOBase::Buffer* buffer)
{
	addref();
	int err = m_sender.async_op(buffer,0);
	if (err != 0)
		release();

	return err;
}

void OOSvrBase::detail::AsyncSocketImpl::on_sent(AsyncIOHelper::AsyncOp* send_op, int err)
{
	if (m_sender.notify_async(send_op,err))
		release();
}
			
int OOSvrBase::detail::AsyncSocketImpl::send(OOBase::Buffer* buffer, const OOBase::timeval_t* timeout)
{
	return m_sender.sync_op(buffer,0,timeout);
}
