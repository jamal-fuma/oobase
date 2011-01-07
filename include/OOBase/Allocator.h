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

#ifndef OOBASE_ALLOCATOR_H_INCLUDED_
#define OOBASE_ALLOCATOR_H_INCLUDED_

#include "../config-base.h"

#include <limits>

#if defined(max)
#undef max
#endif

namespace OOBase
{
	template <typename T>
	class ThrowAllocator
	{
	public:
		// type definitions
		typedef T              value_type;
		typedef T*             pointer;
		typedef const T*       const_pointer;
		typedef T&             reference;
		typedef const T&       const_reference;
		typedef std::size_t    size_type;
		typedef std::ptrdiff_t difference_type;

		// rebind allocator to type U
		template <typename U>
		struct rebind 
		{
			typedef ThrowAllocator<U> other;
		};

		// return address of values
		pointer address(reference value) const 
		{
			return &value;
		}

		const_pointer address(const_reference value) const
		{
			return &value;
		}

		// constructors and destructor
		// - nothing to do because the allocator has no state
		ThrowAllocator() throw() 
		{}

		ThrowAllocator(const ThrowAllocator&) throw() 
		{}

		template <typename U>
		ThrowAllocator (const ThrowAllocator<U>&) throw() 
		{}

		template<typename U>
		ThrowAllocator& operator = (const ThrowAllocator<U>&) throw()
		{
			return *this;
		}

		~ThrowAllocator() throw() 
		{}

		// return maximum number of elements that can be allocated
		size_type max_size() const throw() 
		{
		   return std::numeric_limits<size_type>::max() / sizeof(T);
		}

		// initialize elements of allocated storage p with value value
		void construct(pointer p, const T& value) 
		{
			// initialize memory with placement new
			new (static_cast<void*>(p)) T(value);
		}

		// destroy elements of initialized storage p
		void destroy(pointer p) 
		{
			// destroy objects by calling their destructor
			if (p) 
				p->~T();
		}

		// allocate but don't initialize num elements of type T
		pointer allocate(size_type num, const void* = 0) 
		{
			if (!num)
				return 0;

			void* p = 0;
			if (num > 1)
				p = OOBase::Allocate(num*sizeof(T),1,"OOBase::Allocacator::allocate()",0);
			else
				p = OOBase::Allocate(sizeof(T),0,"OOBase::Allocacator::allocate()",0);
			
			if (!p)
				throw std::bad_alloc();

			return static_cast<pointer>(p);
		}

		// deallocate storage p of deleted elements
		void deallocate(pointer p, size_type num) 
		{
			if (p && num)
			{
				if (num > 1)
					OOBase::Free(p,1);
				else
					OOBase::Free(p,0);
			}	
		}
	};

	// return that all specializations of this allocator are interchangeable
	template <typename T1, typename T2>
	bool operator == (const ThrowAllocator<T1>&, const ThrowAllocator<T2>&) throw() 
	{
		return true;
	}

	template <typename T1, typename T2>
	bool operator != (const ThrowAllocator<T1>&, const ThrowAllocator<T2>&) throw() 
	{
		return false;
	}

	template <typename T>
	class CriticalAllocator
	{
	public:
		// type definitions
		typedef T              value_type;
		typedef T*             pointer;
		typedef const T*       const_pointer;
		typedef T&             reference;
		typedef const T&       const_reference;
		typedef std::size_t    size_type;
		typedef std::ptrdiff_t difference_type;

		// rebind allocator to type U
		template <typename U>
		struct rebind 
		{
			typedef CriticalAllocator<U> other;
		};

		// return address of values
		pointer address(reference value) const 
		{
			return &value;
		}

		const_pointer address(const_reference value) const
		{
			return &value;
		}

		// constructors and destructor
		// - nothing to do because the allocator has no state
		CriticalAllocator() throw() 
		{}

		CriticalAllocator(const CriticalAllocator&) throw() 
		{}

		template <typename U>
		CriticalAllocator (const CriticalAllocator<U>&) throw() 
		{}

		template<typename U>
		CriticalAllocator& operator = (const CriticalAllocator<U>&) throw()
		{
			return *this;
		}

		~CriticalAllocator() throw() 
		{}

		// return maximum number of elements that can be allocated
		size_type max_size() const throw() 
		{
		   return std::numeric_limits<size_type>::max() / sizeof(T);
		}

		// initialize elements of allocated storage p with value value
		void construct(pointer p, const T& value) 
		{
			// initialize memory with placement new
			new (static_cast<void*>(p)) T(value);
		}

		// destroy elements of initialized storage p
		void destroy(pointer p) 
		{
			// destroy objects by calling their destructor
			if (p) 
				p->~T();
		}

		// allocate but don't initialize num elements of type T
		pointer allocate(size_type num, const void* = 0) 
		{
			if (!num)
				return 0;

			void* p = 0;
			if (num > 1)
				p = OOBase::Allocate(num*sizeof(T),1,"OOBase::Allocacator::allocate()",0);
			else
				p = OOBase::Allocate(sizeof(T),0,"OOBase::Allocacator::allocate()",0);
			
			if (!p)
				::OOBase::CallCriticalFailureMem("OOBase::Allocacator::allocate()",0);

			return static_cast<pointer>(p);
		}

		// deallocate storage p of deleted elements
		void deallocate(pointer p, size_type num) 
		{
			if (p && num)
			{
				if (num > 1)
					OOBase::Free(p,1);
				else
					OOBase::Free(p,0);
			}	
		}
	};

	// return that all specializations of this allocator are interchangeable
	template <typename T1, typename T2>
	bool operator == (const CriticalAllocator<T1>&, const CriticalAllocator<T2>&) throw() 
	{
		return true;
	}

	template <typename T1, typename T2>
	bool operator != (const CriticalAllocator<T1>&, const CriticalAllocator<T2>&) throw() 
	{
		return false;
	}
}

#endif // OOBASE_ALLOCATOR_H_INCLUDED_
