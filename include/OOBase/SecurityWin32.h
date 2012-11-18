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

#ifndef OOBASE_SECURITY_WIN32_H_INCLUDED_
#define OOBASE_SECURITY_WIN32_H_INCLUDED_

#if defined(_WIN32)

#include "SmartPtr.h"
#include "Win32.h"

#include <userenv.h>
#include <lm.h>
#include <aclapi.h>

#if defined(__MINGW32__)

#if (__GNUC__ <= 3)
typedef struct _TOKEN_GROUPS_AND_PRIVILEGES
{
	DWORD SidCount;
	DWORD SidLength;
	PSID_AND_ATTRIBUTES Sids;
	DWORD RestrictedSidCount;
	DWORD RestrictedSidLength;
	PSID_AND_ATTRIBUTES RestrictedSids;
	DWORD PrivilegeCount;
	DWORD PrivilegeLength;
	PLUID_AND_ATTRIBUTES Privileges;
	LUID AuthenticationId;
} TOKEN_GROUPS_AND_PRIVILEGES;

extern "C"
WINADVAPI
BOOL
APIENTRY
CreateRestrictedToken(
	HANDLE ExistingTokenHandle,
	DWORD Flags,
	DWORD DisableSidCount,
	PSID_AND_ATTRIBUTES SidsToDisable,
	DWORD DeletePrivilegeCount,
	PLUID_AND_ATTRIBUTES PrivilegesToDelete,
	DWORD RestrictedSidCount,
	PSID_AND_ATTRIBUTES SidsToRestrict,
	PHANDLE NewTokenHandle
);

#ifndef DISABLE_MAX_PRIVILEGE
#define DISABLE_MAX_PRIVILEGE   0x1
#endif

#ifndef SANDBOX_INERT
#define SANDBOX_INERT           0x2
#endif

#endif

#else
// Not available under MinGW
#include <WinSafer.h>
#endif

#endif
#if defined(_WIN32) || defined(DOXYGEN)

namespace OOBase
{
	namespace Win32
	{
		class SIDDestructor
		{
		public:
			typedef CrtAllocator Allocator;

			static void destroy(PSID ptr)
			{
				FreeSid(ptr);
			}
		};

		class sec_descript_t
		{
		public:
			sec_descript_t();
			~sec_descript_t();

			DWORD SetEntriesInAcl(ULONG cCountOfExplicitEntries, PEXPLICIT_ACCESSW pListOfExplicitEntries, PACL OldAcl);

			void* descriptor()
			{
				return m_psd;
			}

		private:
			OOBase::SmartPtr<ACL,OOBase::Win32::LocalAllocDestructor>  m_pACL;
			OOBase::SmartPtr<void,OOBase::Win32::LocalAllocDestructor> m_psd;
		};

		DWORD RestrictToken(HANDLE& hToken);
		DWORD SetTokenDefaultDACL(HANDLE hToken);
		DWORD LoadUserProfileFromToken(HANDLE hToken, HANDLE& hProfile);
		DWORD GetNameFromToken(HANDLE hToken, OOBase::TempPtr<wchar_t>& strUserName, OOBase::TempPtr<wchar_t>& strDomainName);
		DWORD GetLogonSID(HANDLE hToken, OOBase::TempPtr<void>& pSIDLogon);
		DWORD EnableUserAccessToDir(const wchar_t* pszPath, const TOKEN_USER* pUser);
		bool MatchSids(ULONG count, PSID_AND_ATTRIBUTES pSids1, PSID_AND_ATTRIBUTES pSids2);
		bool MatchPrivileges(ULONG count, PLUID_AND_ATTRIBUTES Privs1, PLUID_AND_ATTRIBUTES Privs2);

		template <typename T>
		DWORD GetTokenInfo(HANDLE hToken, TOKEN_INFORMATION_CLASS cls, OOBase::TempPtr<T>& info)
		{
			DWORD dwLen = 0;
			if (GetTokenInformation(hToken,cls,NULL,0,&dwLen))
				return ERROR_SUCCESS;

			DWORD dwErr = GetLastError();
			if (dwErr != ERROR_INSUFFICIENT_BUFFER)
				return dwErr;

			info = static_cast<T*>(info.get_allocator().allocate(dwLen,16));
			if (!info)
				return ERROR_OUTOFMEMORY;

			if (GetTokenInformation(hToken,cls,info,dwLen,&dwLen))
				return ERROR_SUCCESS;

			return GetLastError();
		}
	}
}

#endif // _WIN32

#endif // OOBASE_SECURITY_WIN32_H_INCLUDED_
