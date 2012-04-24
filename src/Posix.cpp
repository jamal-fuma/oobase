///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor, Jamal Natour
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

#include "../include/OOBase/Posix.h"
#include "../include/OOBase/String.h"

#if defined (HAVE_UNISTD_H)

#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>

ssize_t OOBase::POSIX::read(int fd, void* buf, size_t count)
{
	for (;;)
	{
		ssize_t r = ::read(fd,buf,count);
		if (r != -1)
			return r;

		if (errno != EINTR)
			return r;
	}
}

ssize_t OOBase::POSIX::write(int fd, const void* buf, size_t count)
{
	for (;;)
	{
		ssize_t w = ::write(fd,buf,count);
		if (w != -1)
			return w;

		if (errno != EINTR)
			return w;
	}
}

int OOBase::POSIX::close(int fd)
{
	for (;;)
	{
		if (::close(fd) == 0)
			return 0;

		if (errno != EINTR)
			return errno;
	}
}

int OOBase::POSIX::set_close_on_exec(int sock, bool set)
{
	int flags = fcntl(sock,F_GETFD);
	if (flags == -1)
		return errno;

	flags = (set ? flags | FD_CLOEXEC : flags & ~FD_CLOEXEC);
	if (fcntl(sock,F_GETFD,flags) == -1)
		return errno;

	return 0;
}

int OOBase::POSIX::close_file_descriptors(int* except, size_t ex_count)
{
	int mx = getdtablesize();
	if (mx == -1)
	{
#if defined(_POSIX_OPEN_MAX) && (_POSIX_OPEN_MAX >= 0)
#if (_POSIX_OPEN_MAX == 0)
		/* value available at runtime only */
		mx = sysconf(_SC_OPEN_MAX);
#else
		/* value available at compile time */
		mx = _POSIX_OPEN_MAX;
#endif
#endif
	}

	if (mx > 0)
	{
		for (int fd = 0; fd < mx; ++fd)
		{
			bool do_close = true;
			for (size_t e = 0; e < ex_count; ++e)
			{
				if (fd == except[e])
				{
					do_close = false;
					break;
				}
			}

			if (do_close && POSIX::close(fd) != 0 && errno != EBADF)
				return errno;
		}
	}
	else
	{
		/* based on lsof style walk of proc filesystem so should
		 * work on anything with a proc filesystem i.e. a OSx/BSD */
		/* walk proc, closing all descriptors from stderr onwards for our pid */

		OOBase::LocalString str;
		int err = str.printf("/proc/%u/fd/",getpid());
		if (err != 0)
			return err;

		DIR* pdir = opendir(str.c_str());
		if (!pdir)
			return errno;

		/* skips ./ and ../ entries in addition to skipping to the passed fd offset */
		for (dirent* pfile; (pfile = readdir(pdir));)
		{
			if (*pfile->d_name != '.')
			{
				int fd = atoi(pfile->d_name);
				bool do_close = true;
				for (size_t e = 0; e < ex_count; ++e)
				{
					if (fd == except[e])
					{
						do_close = false;
						break;
					}
				}

				if (do_close && POSIX::close(fd) != 0 && errno != EBADF)
					return errno;
			}
		}

		closedir(pdir);
	}

	return 0;
}

#endif
