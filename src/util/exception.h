/*
** exception.h
**
** This file is part of mkxp.
**
** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <string>
#include <stdio.h>
#include <stdarg.h>
#include "mkxp-polyfill.h" // vsnprintf

struct Exception
{
	enum Type
	{
		Ok,

		RGSSError,
		Reset,
		NoFileError,
		IOError,

		/* Already defined by ruby */
		TypeError,
		ArgumentError,
		SystemExit,
		RuntimeError,

		/* New types introduced in mkxp */
		PHYSFSError,
		SDLError,
		MKXPError,
	};

	Type type;
	std::string msg;

	Exception()
	    : type(Ok)
	{}

	Exception(Type type, const char *format, ...)
	    : type(type)
	{
		va_list ap;
		va_start(ap, format);

		msg.resize(512);
		vsnprintf(&msg[0], msg.size(), format, ap);

		va_end(ap);
	}

	constexpr bool is_ok() const noexcept
	{
		return type == Ok;
	}

	constexpr bool is_error() const noexcept
	{
		return !is_ok();
	}

	const char *what() const
	{
		static thread_local std::string buf;
		buf.clear();
		switch (type)
		{
			case Ok: buf.append("Ok: "); break;
			case RGSSError: buf.append("RGSSError: "); break;
			case Reset: buf.append("Reset: "); break;
			case NoFileError: buf.append("NoFileError: "); break;
			case IOError: buf.append("IOError: "); break;
			case TypeError: buf.append("TypeError: "); break;
			case ArgumentError: buf.append("ArgumentError: "); break;
			case SystemExit: buf.append("SystemExit: "); break;
			case RuntimeError: buf.append("RuntimeError: "); break;
			case PHYSFSError: buf.append("PHYSFSError: "); break;
			case SDLError: buf.append("SDLError: "); break;
			case MKXPError: buf.append("MKXPError: "); break;
			default: break;
		}
		buf.append(msg);
		return buf.c_str();
	}
};

#endif // EXCEPTION_H
