//------------------------------------------------------------------------------
//
// $Id: CabCompress.h 72 2002-11-05 09:28:23Z dgehriger $
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
#include "tstring.h"
#include <tchar.h>

class CabCompress
{
public:
    enum Compression
    {
        compNone,
        compMSZIP,
        compLZX,
        compQuantum
    };

    // constructor
    CabCompress(const _TCHAR*   cabFolder, 
                const _TCHAR*   cabTemplate,
                unsigned long   mediaSize, 
                int             cabIndexStart = 1,
                unsigned short  setID = 0,
                const _TCHAR*   diskName = _T(""),
                unsigned int    headerReserved = 0);

    // destructor
    ~CabCompress();

    // add a new file (returns last cabinet index)
    int                 addFile(const _TCHAR* filePath, const _TCHAR* fileName = 0, Compression comp = compMSZIP);

    // begin a new contued cabinet (returns last cabinet index)
    int                 flushCabinet();

    // flush compression window
    int                 flush();

    // callback called for each file
    typedef bool (__stdcall* PFN_CALLBACK)(void* pv, bool extracted, const _TCHAR* entry, size_t size);

    // extract files
    bool                extractTo(const _TCHAR* dstDir, PFN_CALLBACK pfnCallback = 0, void* pv = 0) const;

    // evaluate the cabinet tempalte
    std::string         evalCabTemplate(int index) const;

private:
    struct Impl;
    Impl* m_pImpl;
};
