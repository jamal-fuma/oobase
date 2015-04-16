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

#ifndef OOBASE_TLS_SINGLETON_H_INCLUDED_
#define OOBASE_TLS_SINGLETON_H_INCLUDED_

#include "Memory.h"

namespace OOBase
{
	namespace TLS
	{
		bool Get(const void* key, void** val);
		bool Set(const void* key, void* val, void (*destructor)(void*) = NULL);
	}

	template <typename T, typename DLL>
	class TLSSingleton : public NonCopyable
	{
	public:
		static T* instance()
		{
			void* inst = NULL;
			if (!TLS::Get(&s_sentinal,&inst))
				inst = init();

			return static_cast<T*>(inst);
		}

	private:
		// Prevent creation
		TLSSingleton();
		~TLSSingleton();

		static size_t s_sentinal;

		static void* init()
		{
			T* t = NULL;
			if (OOBase::ThreadLocalAllocator::allocate_new(t))
			{
				int err = TLS::Set(&s_sentinal,t,&destroy);
				if (err != 0)
					OOBase_CallCriticalFailure(err);
			}
			return t;
		}

		static void destroy(void* p)
		{
			if (p)
				OOBase::ThreadLocalAllocator::delete_free(static_cast<T*>(p));
		}
	};

	template <typename T, typename DLL>
	size_t TLSSingleton<T,DLL>::s_sentinal = 1;
}

#endif // OOBASE_TLS_SINGLETON_H_INCLUDED_
