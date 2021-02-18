//------------------------------------------------------------------------
//
//  Eureka DOOM Editor
//
//  Copyright (C) 2020 Ioan Chera
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#ifndef Errors_hpp
#define Errors_hpp

#include "PrintfMacros.h"
#include "m_strings.h"

#include <stdexcept>

//
// Wad read exception
//
class WadReadException : public std::runtime_error
{
public:
	WadReadException(const SString& msg) : std::runtime_error(msg.c_str())
	{
	}
};

[[noreturn]] void ThrowException(EUR_FORMAT_STRING(const char *fmt), ...) EUR_PRINTF(1, 2);

#endif /* Errors_hpp */
