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

#include "../include/OOBase/Memory.h"
#include "../include/OOBase/CmdArgs.h"
#include "../include/OOBase/Win32.h"

#if defined(_WIN32)
#include <shellapi.h>
#endif

int OOBase::CmdArgs::add_option(const char* id, char short_opt, bool has_value, const char* long_opt)
{
	LocalString strId(m_map_opts.get_allocator()),strLongOpt(m_map_opts.get_allocator());
	int err = strId.assign(id);
	if (err == 0)
		err = strLongOpt.assign(long_opt);

	if (err != 0)
		return err;

	if (strId.empty())
		return EINVAL;
	
	Option opt(m_map_opts.get_allocator());
	opt.m_short_opt = short_opt;
	if (long_opt)
		opt.m_long_opt = strLongOpt;
	else
		opt.m_long_opt = strId;

	opt.m_has_value = has_value;

	return m_map_opts.insert(strId,opt);
}

int OOBase::CmdArgs::error(results_t& results, int retval, const char* key, const char* value) const
{
	results.clear();

	LocalString strErr(results.get_allocator()),strVal(results.get_allocator());
	int err = strErr.assign(key);
	if (err == 0)
		err = strVal.assign(value);
	if (err == 0)
		err = results.insert(strErr,strVal);
	if (err == 0)
		err = retval;
					
	return err;
}

#if defined(_WIN32)
int OOBase::CmdArgs::parse(results_t& results, int skip) const
{
	int argc = 0;
	LocalPtr<LPWSTR,Win32::LocalAllocDestructor> argvw = CommandLineToArgvW(GetCommandLineW(),&argc);
	if (!argvw)
		return GetLastError();

	TempPtr<const char*> argv(results.get_allocator());
	if (!argv.reallocate(argc))
		return ERROR_OUTOFMEMORY;

	Vector<LocalString,AllocatorInstance> vecArgv(results.get_allocator());
	for (int i=0;i<argc;++i)
	{
		OOBase::LocalString s(results.get_allocator());
		int err = Win32::wchar_t_to_utf8(argvw[i],s);
		if (err)
			return err;

		argv[i] = s.c_str();
	}

	return parse(argc,argv,results,skip);
}
#endif

int OOBase::CmdArgs::parse(int argc, char* argv[], results_t& results, int skip) const
{
	return parse(argc,(const char**)argv,results,skip);
}

int OOBase::CmdArgs::parse(int argc, const char* argv[], results_t& results, int skip) const
{
	bool bEndOfOpts = false;
	unsigned int pos = 0;
	int err = 0;
	for (int i=skip; i<argc && err==0; ++i)
	{
		if (strcmp(argv[i],"--") == 0)
		{
			// -- Terminator
			bEndOfOpts = true;
			continue;
		}

		if (!bEndOfOpts && argv[i][0]=='-' && argv[i][1] != '\0')
		{
			// Options
			if (argv[i][1] == '-')
			{
				// Long option
				err = parse_long_option(results,argv,i,argc);
			}
			else
			{
				// Short options
				err = parse_short_options(results,argv,i,argc);
			}
		}
		else
		{
			// Arguments
			err = parse_arg(results,argv[i],pos++);
		}
	}

	return err;
}

int OOBase::CmdArgs::parse_long_option(results_t& results, const char** argv, int& arg, int argc) const
{
	LocalString strKey(results.get_allocator()),strVal(results.get_allocator());
	for (size_t i=0; i < m_map_opts.size(); ++i)
	{
		const Option& opt = *m_map_opts.at(i);

		const char* value = "true";
		if (opt.m_long_opt == argv[arg]+2)
		{
			if (opt.m_has_value)
			{
				if (arg >= argc-1)
					return error(results,EINVAL,"missing",argv[arg]);
					
				value = argv[++arg];
			}

			int err = strVal.assign(value);
			if (err != 0)
				return err;

			err = strKey.assign(*m_map_opts.key_at(i));
			if (err)
				return err;

			return results.insert(strKey,strVal);
		}

		if (strncmp(opt.m_long_opt.c_str(),argv[arg]+2,opt.m_long_opt.length())==0 && argv[arg][opt.m_long_opt.length()+2]=='=')
		{
			if (opt.m_has_value)
				value = &argv[arg][opt.m_long_opt.length()+3];

			int err = strVal.assign(value);
			if (err != 0)
				return err;

			err = strKey.assign(*m_map_opts.key_at(i));
			if (err)
				return err;

			return results.insert(strKey,strVal);
		}
	}

	return error(results,ENOENT,"unknown",argv[arg]);
}

int OOBase::CmdArgs::parse_short_options(results_t& results, const char** argv, int& arg, int argc) const
{
	LocalString strKey(results.get_allocator()),strVal(results.get_allocator());
	for (const char* c = argv[arg]+1; *c!='\0'; ++c)
	{
		size_t i;
		for (i = 0; i < m_map_opts.size(); ++i)
		{
			const Option& opt = *m_map_opts.at(i);
			if (opt.m_short_opt == *c)
			{
				if (opt.m_has_value)
				{
					const char* value;
					if (c[1] == '\0')
					{
						// Next arg is the value
						if (arg >= argc-1)
						{
							int err = strVal.printf("-%c",*c);
							if (err)
								return err;

							return error(results,EINVAL,"missing",strVal.c_str());
						}
						
						value = argv[++arg];
					}
					else
						value = &c[1];

					// No more for this arg...
					int err = strVal.assign(value);
					if (err != 0)
						return err;

					err = strKey.assign(*m_map_opts.key_at(i));
					if (err)
						return err;

					return results.insert(strKey,strVal);
				}
				else
				{
					int err = strVal.assign("true");
					if (err != 0)
						return err;

					err = strKey.assign(*m_map_opts.key_at(i));
					if (err)
						return err;

					err = results.insert(strKey,strVal);
					if (err != 0)
						return err;

					break;
				}
			}
		}

		if (i == m_map_opts.size())
		{
			int err = strVal.printf("-%c",*c);
			if (err)
				return err;
				
			return error(results,ENOENT,"unknown",strVal.c_str());
		}
	}

	return 0;
}

int OOBase::CmdArgs::parse_arg(results_t& results, const char* arg, unsigned int position) const
{
	LocalString strArg(results.get_allocator());
	int err = strArg.assign(arg);
	if (err != 0)
		return err;

	LocalString strResult(results.get_allocator());
	if ((err = strResult.printf("@%u",position)) != 0)
		return err;

	return results.insert(strResult,strArg);
}
