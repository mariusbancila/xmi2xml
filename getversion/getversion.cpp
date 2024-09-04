//------------------------------------------------------------------------------
//
// $Id: getversion.cpp 94 2007-09-26 13:51:37Z dgehriger $
//
// Copyright (c) 2001-2004 Daniel Gehriger <gehriger at linkcad dot com>
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
#include "stdafx.h"

//------------------------------------------------------------------------------
int _tmain(int argc, _TCHAR* argv[])
{
    if (argc < 2 || argc > 3)
        return -1;

    _TCHAR* path = argv[1];
    int a = 0, b = 0, c = 0, d = 0;
    if (argc == 3)
    {
        if (_stscanf_s(argv[1], _T("%d.%d.%d.%d"), &a, &b, &c, &d) != 4)
            return -1;

        path = argv[2];
    }

    DWORD dw; 
    DWORD len = GetFileVersionInfoSize(path, &dw );
    if (len == 0) return -1;

    std::vector<_TCHAR> buf(len);
    GetFileVersionInfo(path, 0, len, &buf[0]);
    unsigned int vlen;
    LPVOID lpvi;
    VerQueryValue(&buf[0], _T("\\"), &lpvi, &vlen);

    VS_FIXEDFILEINFO fileInfo;
    fileInfo = *reinterpret_cast<VS_FIXEDFILEINFO*>(lpvi);
    tostringstream s;
    s << a + (unsigned)HIWORD(fileInfo.dwFileVersionMS) << _T(".")
      << b + (unsigned)LOWORD(fileInfo.dwFileVersionMS) << _T(".")
      << c + (unsigned)HIWORD(fileInfo.dwFileVersionLS) << _T(".")
      << d + (unsigned)LOWORD(fileInfo.dwFileVersionLS);

    tcout << s.str() << std::endl;
    return 0;
}

