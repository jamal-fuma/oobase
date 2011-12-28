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

#include "../include/OOBase/TimeVal.h"

#include "tr24731.h"

#include <time.h>

#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif

#include <limits.h>

const OOBase::timeval_t OOBase::timeval_t::MaxTime(LLONG_MAX, 999999);
const OOBase::timeval_t OOBase::timeval_t::Zero(0, 0);

OOBase::timeval_t OOBase::timeval_t::now()
{
#if defined(_WIN32)
	
	static LONGLONG freq = -1LL;
	if (freq < 0)
	{
		LARGE_INTEGER liFreq;
		if (QueryPerformanceFrequency(&liFreq))
			freq = liFreq.QuadPart;
		else
			freq = 0;
	}

	if (freq > 0)
	{
		LARGE_INTEGER liNow;
		if (!QueryPerformanceCounter(&liNow))
			OOBase_CallCriticalFailure(GetLastError());

		return OOBase::timeval_t(liNow.QuadPart / freq,static_cast<int>(liNow.QuadPart % freq));
	}
	
#if (_WIN32_WINNT >= 0x0600)
	ULONGLONG ullTicks = GetTickCount64();
	return OOBase::timeval_t(ullTicks / 1000ULL,static_cast<int>(ullTicks % 1000ULL));
#endif

	DWORD dwTicks = GetTickCount();
	return OOBase::timeval_t(dwTicks / 1000,static_cast<int>(dwTicks % 1000));

#elif defined(HAVE_SYS_TIME_H) && (HAVE_SYS_TIME_H == 1)

	timeval tv;
	::gettimeofday(&tv,NULL);

	return OOBase::timeval_t(tv.tv_sec,tv.tv_usec);

#else
#error now() !!
#endif
}

OOBase::timeval_t& OOBase::timeval_t::operator += (const timeval_t& rhs)
{
	if (m_tv_usec + rhs.m_tv_usec >= 1000000)
	{
		int nsec = (m_tv_usec + rhs.m_tv_usec) / 1000000;
		m_tv_usec -= 1000000 * nsec;
		m_tv_sec += nsec;
	}

	m_tv_sec += rhs.m_tv_sec;
	m_tv_usec += rhs.m_tv_usec;

	return *this;
}

OOBase::timeval_t& OOBase::timeval_t::operator -= (const timeval_t& rhs)
{
	/* Perform the carry for the later subtraction by updating r. */
	timeval_t r = rhs;
	if (m_tv_usec < r.m_tv_usec)
	{
		int nsec = (r.m_tv_usec - m_tv_usec) / 1000000 + 1;
		r.m_tv_usec -= 1000000 * nsec;
		r.m_tv_sec += nsec;
	}

	if (m_tv_usec - r.m_tv_usec >= 1000000)
	{
		int nsec = (m_tv_usec - r.m_tv_usec) / 1000000;
		r.m_tv_usec += 1000000 * nsec;
		r.m_tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
	 m_tv_usec is certainly positive. */
	m_tv_sec -= r.m_tv_sec;
	m_tv_usec -= r.m_tv_usec;

	return *this;
}

OOBase::timeval_t OOBase::timeval_t::deadline(unsigned long msec)
{
	return now() + timeval_t(msec / 1000,(msec % 1000) * 1000);
}

OOBase::Countdown::Countdown(const timeval_t* timeout) :
		m_null(timeout == NULL),
		m_end(timeout ? timeval_t::now() + *timeout : timeval_t::Zero)
{
}

OOBase::Countdown::Countdown(timeval_t::time_64_t s, int us) :
		m_null(false),
		m_end(timeval_t::now() + timeval_t(s,us))
{
}

OOBase::Countdown::Countdown(const Countdown& rhs) :
		m_null(rhs.m_null),
		m_end(rhs.m_end)
{
}

OOBase::Countdown& OOBase::Countdown::operator = (const Countdown& rhs)
{
	if (this != &rhs)
	{
		m_null = rhs.m_null;
		m_end = rhs.m_end;
	}
	return *this;
}

bool OOBase::Countdown::has_ended() const
{
	if (m_null)
		return false;

	return timeval_t::now() >= m_end;
}

void OOBase::Countdown::timeout(timeval_t& timeout) const
{
	if (m_null)
		timeout = timeval_t::MaxTime;
	else
	{
		timeval_t now = timeval_t::now();
		if (now >= m_end)
			timeout = timeval_t::Zero;
		else
			timeout = (m_end - now);
	}
}

void OOBase::Countdown::timeval(::timeval& timeout) const
{
	timeval_t to;
	this->timeout(to);

	timeout.tv_sec = static_cast<long>(to.tv_sec());
	timeout.tv_usec = to.tv_usec();
}

#if defined(_WIN32)
OOBase::Countdown::Countdown(DWORD millisecs) :
		m_null(millisecs == INFINITE),
		m_end(millisecs == INFINITE ? timeval_t::Zero : timeval_t::now() + timeval_t(millisecs / 1000, (millisecs % 1000) * 1000))
{
}

DWORD OOBase::Countdown::msec() const
{
	if (m_null)
		return INFINITE;

	timeval_t now = timeval_t::now();
	if (now >= m_end)
		return 0;
		
	return (m_end - now).msec();
}
#else

void OOBase::Countdown::abs_timespec(timespec& timeout) const
{
	timeout.tv_sec = m_end.tv_sec();
	timeout.tv_nsec = m_end.tv_usec() * 1000;
}

#endif
