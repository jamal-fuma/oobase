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

#ifndef OOBASE_ONCE_H_INCLUDED_
#define OOBASE_ONCE_H_INCLUDED_

#include "Mutex.h"

#if defined(_WIN32) && (_WIN32_WINNT < 0x0600)
typedef union 
{       
	PVOID Ptr;
} INIT_ONCE;
#endif

namespace OOBase
{
	namespace Once
	{
#if defined(DOXYGEN)
		/// The platform specific 'once' type.
		struct once_t {};
		
		/** \def ONCE_T_INIT
		 *  The platform specific 'once' type initialiser
		 */
		#define ONCE_T_INIT
#elif defined(_WIN32)
		typedef INIT_ONCE once_t;
		#define ONCE_T_INIT {0}
#elif defined(HAVE_PTHREAD)
		typedef pthread_once_t once_t;
		#define ONCE_T_INIT PTHREAD_ONCE_INIT
#else
#error Implement platform native one-time functions
#endif

		typedef void (*pfn_once)(void);

		void Run(once_t* key, pfn_once fn);
	}
}

#endif // OOBASE_ONCE_H_INCLUDED_
