///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2013 Rick Taylor
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

#include "../include/OOBase/File.h"
#include "../include/OOBase/Win32.h"

#if defined(HAVE_UNISTD_H)
#include <sys/stat.h>
#endif

OOBase::File::File()
{
}

OOBase::File::~File()
{
}

int OOBase::File::open(const char* filename, bool writeable)
{
	if (!filename)
		return EINVAL;

#if defined(_WIN32)
	ScopedArrayPtr<wchar_t> wname;
	int err = Win32::utf8_to_wchar_t(filename,wname);
	if (err)
		return err;

	DWORD dwAccess = GENERIC_READ;
	DWORD dwDisp = OPEN_EXISTING;
	if (writeable)
	{
		dwAccess |= GENERIC_WRITE;
		dwDisp = OPEN_ALWAYS;
	}

	m_fd = ::CreateFileW(wname.get(),dwAccess,FILE_SHARE_READ,NULL,dwDisp,FILE_ATTRIBUTE_NORMAL,NULL);
	if (!m_fd.is_valid())
		return ::GetLastError();

#elif defined(HAVE_UNISTD_H)

	int flags = O_RDONLY;
	if (writeable)
		flags = O_RDWR | O_CREAT;
		
	m_fd = POSIX::open(filename,flags,0664);
	if (!m_fd.is_valid())
		return errno;
#endif
	return 0;
}

size_t OOBase::File::read(void* p, size_t size)
{
#if defined(_WIN32)
	size_t total = 0;
	while (size)
	{
		DWORD r = 0;
		if (!::ReadFile(m_fd,p,static_cast<DWORD>(size),&r,NULL))
			return size_t(-1);
		total += r;
		if (r <= size)
			break;
		p = static_cast<uint8_t*>(p) + r;
		size -= r;
	}
	return total;
#elif defined(HAVE_UNISTD_H)
	return POSIX::read(m_fd,p,size);
#endif
}

size_t OOBase::File::read(const RefPtr<Buffer>& buffer, int& err, size_t len)
{
	if (!buffer)
	{
		err = EINVAL;
		return size_t(-1);
	}

	if (!len)
		len = buffer->space();

	size_t r = read(buffer->wr_ptr(),len);
	if (r == size_t(-1))
	{
#if defined(_WIN32)
		err = ::GetLastError();
#elif defined(HAVE_UNISTD_H)
		err = errno;
#endif
	}
	else
		buffer->wr_ptr(r);

	return r;
}

size_t OOBase::File::write(const void* p, size_t size)
{
#if defined(_WIN32)
	size_t total = 0;
	while (size)
	{
		DWORD w = 0;
		if (!::WriteFile(m_fd,p,static_cast<DWORD>(size),&w,NULL))
			return size_t(-1);

		total += w;
		size -= w;
		p = static_cast<const uint8_t*>(p) + w;
	}
	return total;
#elif defined(HAVE_UNISTD_H)
	return POSIX::write(m_fd,p,size);
#endif
}

int OOBase::File::write(const RefPtr<Buffer>& buffer, size_t len)
{
	if (!buffer)
		return EINVAL;

	if (!len)
		len = buffer->length();

	size_t w = write(buffer->rd_ptr(),len);
	if (w == size_t(-1))
	{
#if defined(_WIN32)
		return ::GetLastError();
#elif defined(HAVE_UNISTD_H)
		return errno;
#endif
	}
		
	buffer->rd_ptr(w);
	return 0;
}

OOBase::uint64_t OOBase::File::seek(int64_t offset, enum seek_direction dir)
{
#if defined(_WIN32)
	DWORD d = FILE_CURRENT;
	switch (dir)
	{
	case seek_begin:
		d = FILE_BEGIN;
		break;

	case seek_current:
		break;

	case seek_end:
		d = FILE_END;
		break;
	}
	LARGE_INTEGER off,pos;
	off.QuadPart = offset;
	pos.QuadPart = 0;
	if (!::SetFilePointerEx(m_fd,off,&pos,d))
		return uint64_t(-1);
	return pos.QuadPart;
#else
	int d = SEEK_CUR;
	switch (dir)
	{
	case seek_begin:
		d = SEEK_SET;
		break;

	case seek_current:
		break;

	case seek_end:
		d = SEEK_END;
		break;
	}
	off_t s = ::lseek(m_fd,offset,d);
	if (s == off_t(-1))
		return uint64_t(-1);
	return s;
#endif
}

OOBase::uint64_t OOBase::File::tell() const
{
#if defined(_WIN32)
	LARGE_INTEGER off,pos;
	off.QuadPart = 0;
	pos.QuadPart = 0;
	if (!::SetFilePointerEx(m_fd,off,&pos,FILE_CURRENT))
		return uint64_t(-1);
	return pos.QuadPart;
#else
	off_t s = ::lseek(m_fd,0,SEEK_CUR);
	if (s == off_t(-1))
		return uint64_t(-1);
	return s;
#endif
}

OOBase::uint64_t OOBase::File::length() const
{
#if defined(_WIN32)
	LARGE_INTEGER li;
	li.QuadPart = 0;
	if (!GetFileSizeEx(m_fd,&li))
		return uint64_t(-1);

	return li.QuadPart;
#else
	struct stat s = {0};
	int err = ::fstat(m_fd,&s);
	if (err)
		return uint64_t(-1);

	return s.st_size;
#endif
}
