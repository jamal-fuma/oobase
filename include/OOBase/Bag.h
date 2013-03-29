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

#ifndef OOBASE_BAG_H_INCLUDED_
#define OOBASE_BAG_H_INCLUDED_

#include "Memory.h"

namespace OOBase
{
	namespace detail
	{
		template <typename Allocator, typename T>
		class PODBagBase : public Allocating<Allocator>, public NonCopyable
		{
			typedef Allocating<Allocator> baseClass;

		public:
			PODBagBase() : baseClass(), m_data(NULL), m_size(0), m_capacity(0)
			{}

			PODBagBase(AllocatorInstance& allocator) : baseClass(allocator), m_data(NULL), m_size(0), m_capacity(0)
			{}

		protected:
			T*     m_data;
			size_t m_size;
			size_t m_capacity;
		};

		template <typename Allocator, typename T, bool POD = false>
		class PODBag : public PODBagBase<Allocator,T>
		{
			typedef PODBagBase<Allocator,T> baseClass;

		public:
			PODBag() : baseClass()
			{}

			PODBag(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			void clear()
			{
				while (this->m_size > 0)
					this->m_data[--this->m_size].~T();
			}

		protected:
			int insert_at(size_t pos, const T& value)
			{
				if (this->m_size+1 > this->m_capacity)
				{
					size_t capacity = (this->m_capacity == 0 ? 4 : this->m_capacity*2);
					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

#if defined(OOBASE_HAVE_EXCEPTIONS)
					size_t i = 0;
					try
					{
						for (;i<this->m_size;++i)
							::new (&new_data[i + (i<pos ? 0 : 1)]) T(this->m_data[i]);

						::new (&new_data[pos]) T(value);
					}
					catch (...)
					{
						while (i-- > 0)
						{
							if (i != pos)
								new_data[i].~T();
						}
						baseClass::free(new_data);
						throw;
					}

					for (i=0;i<this->m_size;++i)
						this->m_data[i].~T();

#else
					for (size_t i = 0;i<this->m_size;++i)
					{
						::new (&new_data[i + (i<pos ? 0 : 1)]) T(this->m_data[i]);
						this->m_data[i].~T();
					}

					::new (&new_data[pos]) T(value);
#endif
					baseClass::free(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				else
				{
					if (pos < this->m_size)
					{
						// Shuffle the contents down the buffer
						::new (&this->m_data[this->m_size]) T(this->m_data[this->m_size-1]);
						for (size_t i = this->m_size-1; i > pos; --i)
							this->m_data[i] = this->m_data[i-1];

						this->m_data[pos] = value;
					}
					else
						::new (&this->m_data[this->m_size]) T(value);
				}
				++this->m_size;
				return 0;
			}

			void remove_at(size_t pos, bool sorted)
			{
				if (this->m_data && pos < this->m_size)
				{
					if (sorted)
					{
						for(--this->m_size;pos < this->m_size;++pos)
							this->m_data[pos] = this->m_data[pos+1];
					}
					else
						this->m_data[pos] = this->m_data[--this->m_size];

					this->m_data[this->m_size].~T();
				}
			}
		};

		template <typename Allocator, typename T>
		class PODBag<Allocator,T,true> : public PODBagBase<Allocator,T>
		{
			typedef PODBagBase<Allocator,T> baseClass;

		public:
			PODBag() : baseClass()
			{}

			PODBag(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			void clear()
			{
				this->m_size = 0;
			}

		protected:
			int insert_at(size_t pos, const T& value)
			{
				if (this->m_size+1 > this->m_capacity)
				{
					size_t capacity = (this->m_capacity == 0 ? 4 : this->m_capacity*2);

					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					if (pos < this->m_size)
						memcpy(&new_data[pos + 1],&this->m_data[pos],(this->m_size - pos) * sizeof(T));
					else
						pos = this->m_size;

					if (pos > 0)
						memcpy(new_data,&this->m_data,pos * sizeof(T));

					new_data[pos] = value;

					baseClass::free(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				else
				{
					if (pos < this->m_size)
						memmove(&this->m_data[pos + 1],&this->m_data[pos],(this->m_size - pos) * sizeof(T));
					else
						pos = this->m_size;

					this->m_data[pos] = value;
				}
				++this->m_size;
				return 0;
			}

			void remove_at(size_t pos, bool sorted)
			{
				if (this->m_data && pos < this->m_size)
				{
					--this->m_size;
					if (pos < this->m_size)
					{
						if (sorted)
							memmove(&this->m_data[pos],&this->m_data[pos+1],(this->m_size - pos) * sizeof(T));
						else
							this->m_data[pos] = this->m_data[this->m_size+1];
					}
				}
			}
		};

		template <typename T, typename Allocator>
		class BagImpl : public PODBag<Allocator,T,is_pod<T>::value>
		{
			typedef PODBag<Allocator,T,is_pod<T>::value> baseClass;

		public:
			BagImpl() : baseClass()
			{}

			BagImpl(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			~BagImpl()
			{
				destroy();
			}

			bool empty() const
			{
				return (this->m_size == 0);
			}

			size_t size() const
			{
				return this->m_size;
			}

		protected:
			int push(const T& value)
			{
				return baseClass::insert_at(size_t(-1),value);
			}

			bool pop(T* value = NULL)
			{
				if (this->m_size == 0)
					return false;

				if (value)
					*value = this->m_data[this->m_size-1];

				this->m_data[--this->m_size].~T();
				return true;
			}

			T* at(size_t pos)
			{
				return (pos >= this->m_size ? NULL : &this->m_data[pos]);
			}

			const T* at(size_t pos) const
			{
				return (pos >= this->m_size ? NULL : &this->m_data[pos]);
			}

			void destroy()
			{
				baseClass::clear();
				this->free(this->m_data);
			}
		};
	}

	template <typename T, typename Allocator = CrtAllocator>
	class Bag : public detail::BagImpl<T,Allocator>
	{
		typedef detail::BagImpl<T,Allocator> baseClass;

	public:
		Bag() : baseClass()
		{}

		Bag(AllocatorInstance& allocator) : baseClass(allocator)
		{}

		int add(const T& value)
		{
			return baseClass::push(value);
		}

		bool remove(const T& value)
		{
			// This is just really useful!
			for (size_t pos = 0;pos < this->m_size;++pos)
			{
				if (this->m_data[pos] == value)
				{
					remove_at(pos);
					return true;
				}
			}

			return false;
		}

		bool pop(T* value = NULL)
		{
			return baseClass::pop(value);
		}

		void remove_at(size_t pos)
		{
			baseClass::remove_at(pos,false);
		}

		T* at(size_t pos)
		{
			return baseClass::at(pos);
		}

		const T* at(size_t pos) const
		{
			return baseClass::at(pos);
		}
	};
}

#endif // OOBASE_BAG_H_INCLUDED_
