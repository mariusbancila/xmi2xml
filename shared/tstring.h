//------------------------------------------------------------------------------
//
// $Id: tstring.h 97 2007-09-26 17:49:04Z dgehriger $
//
// Copyright (c) 2001-2007 Daniel Gehriger <gehriger at linkcad dot com>
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
#ifndef TSTRING_H_INCLUDED
#define TSTRING_H_INCLUDED
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <tchar.h>
#include <sstream>
#include <string>
#include <vector>
#include <iosfwd>

//------------------------------------------------------------------------------
// Define _UNICODE aware string classes
//------------------------------------------------------------------------------
typedef std::basic_istringstream<_TCHAR> tistringstream;
typedef std::basic_ostringstream<_TCHAR> tostringstream;
typedef std::basic_string<_TCHAR> tstring;
typedef std::vector<_TCHAR> tcharvector;
typedef std::basic_ostream<_TCHAR, std::char_traits<_TCHAR> > tostream;

#ifdef _UNICODE
#define tcerr std::wcerr
#define tcout std::wcout
#else
#define tcerr std::cerr
#define tcout std::cout
#endif

#endif // TSTRING_H_INCLUDED
