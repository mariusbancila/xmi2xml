//------------------------------------------------------------------------------
//
// $Id: StdAfx.h 104 2007-09-26 21:47:39Z dgehriger $
//
// Copyright (c) 2001 Daniel Gehriger <gehriger at linkcad dot com>
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
#ifndef AFX_STDAFX_H__9FAA5F6C_CEAE_44AD_9402_9B55EE04270F__INCLUDED_
#define AFX_STDAFX_H__9FAA5F6C_CEAE_44AD_9402_9B55EE04270F__INCLUDED_

#if defined (_UNICODE) && !defined(UNICODE)
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#pragma warning(disable : 4786) // disable warnings about debug info
#pragma warning(disable : 4290) // disable exception spec warning

#include "..\shared\version.h"

#include <windows.h>
#include <assert.h>
#include <objbase.h>

//------------------------------------------------------------------------------
// MS-XML include files
//------------------------------------------------------------------------------
#if _MSC_VER >= 1200
#import <msxml6.dll> rename_namespace("xml")
//       ^^^^^^^^^^
// If this import statement fails, you need to install the MSXML 4.0
// component from :
//
// <http://tinyurl.com/2um5v7>
#else
#include "msxml6.tlh"
#endif

//------------------------------------------------------------------------------
// MSI include files
//------------------------------------------------------------------------------
#if (_WIN32_MSI < 110)
#define _WIN32_MSI 110
#endif 

#include <Msi.h> 
//       ^^^^^^^
// If this include statement fails, you need to download and install
// the Windows Installer SDK, whic is included in the Windows Platform SDK
// or can be downloaded from :
//
// http://msdn.microsoft.com/code/sample.asp?url=/msdn-files/027/001/457/msdncompositedoc.xml
//
// You also need to add the include file and library search path
// to Visual C++'s list of directories (Tools > Options... > Directories).
#include <Msiquery.h>
#include <MsiDefs.h>
#pragma comment(lib, "msi.lib")

//------------------------------------------------------------------------------
// WinInet include file
//------------------------------------------------------------------------------
#include <Wininet.h>
#pragma comment(lib, "Wininet.lib")

//------------------------------------------------------------------------------
// RPC runtime library
//------------------------------------------------------------------------------
#include <RpcDce.h>
#pragma comment(lib, "Rpcrt4.lib")

//------------------------------------------------------------------------------
// Define smart handles
//------------------------------------------------------------------------------
//>>>> If you are getting error messages from the compiler, see       <<<<
//>>>> http://support.microsoft.com/support/kb/articles/Q216/7/22.asp <<<<
//>>>> And install the latest VisualStudio Service Pack.              <<<<
#include "smrthandle.h"
typedef SmrtHandle<HANDLE, BOOL, CloseHandle>               SmrtFileHandle;
typedef SmrtHandle<HANDLE, BOOL, FindClose>                 SmrtFindHandle;
typedef SmrtHandle<MSIHANDLE, UINT, MsiCloseHandle>         SmrtMsiHandle;
typedef SmrtHandle<HINTERNET, BOOL, InternetCloseHandle>    SmrtHInternet;
typedef SmrtHandle<LPCVOID, BOOL, UnmapViewOfFile>          SmrtFileMap;
typedef SmrtHandle<HMODULE, BOOL, FreeLibrary>              SmrtLibHandle;

//------------------------------------------------------------------------------
// System includes
//------------------------------------------------------------------------------
#include <vector>
#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <utility>
#include <functional>
#include <map>
#include <string.h>
#include <errno.h>

//------------------------------------------------------------------------------
// Unicode aware string classes
//------------------------------------------------------------------------------
#include "tstring.h"

//------------------------------------------------------------------------------
// Fix "for" scoping rules to conform to ANSI C++
//------------------------------------------------------------------------------
#if _MSC_VER <= 1200
#define for if(0)__assume(0);else for
#endif

//------------------------------------------------------------------------------
// Helper function for return code validation
//------------------------------------------------------------------------------
#ifdef _DEBUG
#include <crtdbg.h>
#define OK(hr) _ASSERTE(hr == ERROR_SUCCESS)
#else
inline void OK(HRESULT hr) throw(_com_error)
{
    if (hr != ERROR_SUCCESS) 
    {
        _com_issue_error(hr);
    }
}
#endif

//{{AFX_INSERT_LOCATION}}

#endif // AFX_STDAFX_H__9FAA5F6C_CEAE_44AD_9402_9B55EE04270F__INCLUDED_
