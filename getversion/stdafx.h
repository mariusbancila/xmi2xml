//------------------------------------------------------------------------------
//
// $Id: stdafx.h 72 2002-11-05 09:28:23Z dgehriger $
//
// Copyright (c) 2001-2002 Daniel Gehriger <gehriger at linkcad dot com>
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//------------------------------------------------------------------------------
#pragma once

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include <tchar.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <string.h>

//------------------------------------------------------------------------------
// Define UNICODE aware classes
//------------------------------------------------------------------------------
typedef std::basic_istringstream<_TCHAR> tistringstream;
typedef std::basic_ostringstream<_TCHAR> tostringstream;
typedef std::vector<_TCHAR> tcharvector;
#ifdef UNICODE
#define tcerr std::wcerr
#define tcout std::wcout
#else
#define tcerr std::cerr
#define tcout std::cout
#endif

