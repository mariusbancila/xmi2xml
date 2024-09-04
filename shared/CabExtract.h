//------------------------------------------------------------------------------
//
// $Id: CabExtract.h 72 2002-11-05 09:28:23Z dgehriger $
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
#ifndef CAB_EXTRACT_H_INCLUDED
#define CAB_EXTRACT_H_INCLUDED
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <tchar.h>

class CabExtract
{
public:
    // constructor
    CabExtract(const _TCHAR* cabPath);

    // destructor
    ~CabExtract();

    // callback called for each file (pv is the transparent pointer passed to 'extractTo()')
    typedef bool (__stdcall* PFN_CALLBACK)(void* pv, bool extracted, const _TCHAR* entry, size_t size);

    // extract files
    bool                extractTo(const _TCHAR* dstDir, PFN_CALLBACK pfnCallback = 0, void* pv = 0) const;

private:
    struct Impl;
    Impl* m_pImpl;
};

#endif // CAB_EXTRACT_H_INCLUDED
