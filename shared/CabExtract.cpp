//------------------------------------------------------------------------------
//
// $Id: CabExtract.cpp 94 2007-09-26 13:51:37Z dgehriger $
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
#include "..\shared\version.h"
#include "CabExtract.h"
#include "tstring.h"
#include <windows.h>
#include <fdi.h>
#include <stdlib.h>
#include <malloc.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdexcept>

#include <crtdbg.h>
#include <atlexcept.h>
#include <atlconv.h>
#include <atlbase.h>
#ifdef _DEBUG
#pragma comment(lib, "atls.lib")
#endif

#pragma comment(lib, "Cabinet.lib")

using namespace std;

//------------------------------------------------------------------------------
struct CabExtract::Impl
{
    HFDI                hfdi;
    ERF                 erf;
    string              cabDir;
    string              cabName;
    string              dstDir;
    PFN_CALLBACK        pfnCallback;
    void*               pv;


    static bool         createDirectoryPath(const char* path);
    static const char*  fdierrorToString(int err);
    static FNALLOC(alloc);
    static FNFREE(free);
    static FNOPEN(open);
    static FNREAD(read);
    static FNWRITE(write);
    static FNCLOSE(close);
    static FNSEEK(seek);
    static FNFDINOTIFY(notifyStub);
    FNFDINOTIFY(notify);
};

//------------------------------------------------------------------------------
CabExtract::CabExtract(const _TCHAR* cabPath) :
    m_pImpl(new Impl)
{
    // initialize members
    m_pImpl->hfdi = NULL;

    // try creating error context
    m_pImpl->hfdi = 
        FDICreate(
            Impl::alloc, 
            Impl::free, 
            Impl::open, 
            Impl::read, 
            Impl::write, 
            Impl::close, 
            Impl::seek,
            cpuUNKNOWN,
            &m_pImpl->erf);

    if (m_pImpl->hfdi == NULL)
        throw runtime_error(Impl::fdierrorToString(m_pImpl->erf.erfOper));

    // check if cabinet
    INT_PTR hf;
    ATL::CT2A cabPathA(cabPath);
    hf = Impl::open(cabPathA, _O_BINARY | _O_RDONLY | _O_SEQUENTIAL, 0);
    if (hf == -1)
    {
        string msg = string("Unable to open '") + static_cast<char*>(cabPathA) + string("' for input");
        throw runtime_error(msg.c_str());
    }

    FDICABINETINFO fdici;
    if (!FDIIsCabinet(m_pImpl->hfdi, hf, &fdici))
    {
        Impl::close(hf);
        string msg = string("Not a cabinet file: '") + static_cast<char*>(cabPathA) + string("'");
        throw runtime_error(msg.c_str());
    }
    Impl::close(hf);

    // extract path and file component
    char cabDir[_MAX_PATH];
    char cabName[_MAX_FNAME];
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    char fname[_MAX_FNAME];
    char ext[_MAX_EXT];
    _splitpath_s(cabPathA, 
                 drive, ARRAYSIZE(drive), 
                 dir, ARRAYSIZE(dir), 
                 fname, ARRAYSIZE(fname),
                 ext, ARRAYSIZE(ext));
    _makepath_s(cabName, ARRAYSIZE(cabName), NULL, NULL, fname, ext);
    m_pImpl->cabName = cabName;
    _makepath_s(cabDir, ARRAYSIZE(cabDir), drive, dir, NULL, NULL);
    m_pImpl->cabDir = cabDir;
}

//------------------------------------------------------------------------------
bool CabExtract::extractTo(const _TCHAR*    dstDir, 
                           PFN_CALLBACK     pfnCallback /* = 0 */, 
                           void*            pv /* = 0 */) const
{
    m_pImpl->dstDir         = ATL::CT2A(dstDir);
    m_pImpl->pfnCallback    = pfnCallback;
    m_pImpl->pv             = pv;

    return FDICopy(m_pImpl->hfdi, 
                   const_cast<char*>(m_pImpl->cabName.c_str()), 
                   const_cast<char*>(m_pImpl->cabDir.c_str()), 
                   0, 
                   Impl::notifyStub, 
                   0,
                   reinterpret_cast<void*>(m_pImpl)) ? true : false;
}

//------------------------------------------------------------------------------
CabExtract::~CabExtract()
{
    if (m_pImpl->hfdi)
    {
        FDIDestroy(m_pImpl->hfdi);
    }

    delete m_pImpl;
}

//------------------------------------------------------------------------------
FNFDINOTIFY(CabExtract::Impl::notifyStub)
{
    return reinterpret_cast<CabExtract::Impl*>(pfdin->pv)->notify(fdint, pfdin);
}

//------------------------------------------------------------------------------
FNFDINOTIFY(CabExtract::Impl::notify)
{
    switch (fdint)
    {
        case fdintCOPY_FILE: {	// file to be copied

            if (pfnCallback && !pfnCallback(pv, false, ATL::CA2T(pfdin->psz1), pfdin->cb))
                return 0; // don't extract

            // build up full target path
            char targetPath[_MAX_PATH];
            _makepath_s(targetPath, ARRAYSIZE(targetPath), NULL, dstDir.c_str(), pfdin->psz1, NULL);

            // extract directory component
            char targetDir[_MAX_PATH];
            char drive[_MAX_DRIVE];
            char dir[_MAX_DIR];
            _splitpath_s(targetPath, drive, ARRAYSIZE(drive), dir, ARRAYSIZE(dir), NULL, 0, NULL, 0);
            _makepath_s(targetDir, ARRAYSIZE(targetDir), drive, dir, NULL, NULL);
            if (!createDirectoryPath(targetDir))
                return 0;

            // make sure target is writable
            DWORD attr = GetFileAttributesA(targetPath);
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY))
            {
                attr &= ~FILE_ATTRIBUTE_READONLY;
                SetFileAttributesA(targetPath, attr);
            }

            return Impl::open(targetPath, _O_BINARY|_O_CREAT|_O_WRONLY|_O_SEQUENTIAL, _S_IREAD|_S_IWRITE);
            break; }


        case fdintCLOSE_FILE_INFO: {	// close the file, set relevant info

            // build up full target path
            char targetPath[_MAX_PATH];
            _makepath_s(targetPath, ARRAYSIZE(targetPath), NULL, dstDir.c_str(), pfdin->psz1, NULL);

            // close file
            close(pfdin->hf);

            // set time/date
            HANDLE handle = CreateFileA(targetPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

            if (handle != INVALID_HANDLE_VALUE)
            {
                FILETIME dt;
                if (DosDateTimeToFileTime(pfdin->date, pfdin->time, &dt))
                {
                    FILETIME lft;
                    if (LocalFileTimeToFileTime(&dt, &lft))
                    {
                        SetFileTime(handle, &lft, NULL, &lft);
                    }
                }
                CloseHandle(handle);
            }

            // set attributes
            DWORD attrs = 0;
            if (pfdin->attribs & _A_RDONLY) attrs |= FILE_ATTRIBUTE_READONLY;
            if (pfdin->attribs & _A_SYSTEM) attrs |= FILE_ATTRIBUTE_SYSTEM;
            if (pfdin->attribs & _A_HIDDEN) attrs |= FILE_ATTRIBUTE_HIDDEN;
            if (pfdin->attribs & _A_ARCH)   attrs |= FILE_ATTRIBUTE_ARCHIVE;
            SetFileAttributesA(targetPath, attrs);

            if (pfnCallback)
            {
                pfnCallback(pv, true, ATL::CA2T(pfdin->psz1), pfdin->cb);
            }


            return TRUE; 
            break; }

        case fdintNEXT_CABINET:
            if (pfdin->fdie == FDIERROR_WRONG_CABINET)
            {
                return -1;
            }
            return 0;

        default:
            return 0;
    }
}

//------------------------------------------------------------------------------
FNALLOC(CabExtract::Impl::alloc)
{
    return ::malloc(cb);
}

//------------------------------------------------------------------------------
FNFREE(CabExtract::Impl::free)
{
    ::free(pv);
}

//------------------------------------------------------------------------------
FNOPEN(CabExtract::Impl::open)
{
    int fh;
    ::_sopen_s(&fh, pszFile, oflag, _SH_DENYNO, pmode);
    return fh;
}

//------------------------------------------------------------------------------
FNREAD(CabExtract::Impl::read)
{
    return ::_read(hf, pv, cb);
}

//------------------------------------------------------------------------------
FNWRITE(CabExtract::Impl::write)
{
    return ::_write(hf, pv, cb);
}

//------------------------------------------------------------------------------
FNCLOSE(CabExtract::Impl::close)
{
    return ::_close(hf);
}

//------------------------------------------------------------------------------
FNSEEK(CabExtract::Impl::seek)
{
    return ::_lseek(hf, dist, seektype);
}

//------------------------------------------------------------------------------
const char* CabExtract::Impl::fdierrorToString(int err)
{
    switch (err)
    {
    case FDIERROR_NONE: 
        return "No error";

    case FDIERROR_CABINET_NOT_FOUND:
        return "Cabinet not found";

    case FDIERROR_NOT_A_CABINET:
        return "Not a cabinet";

    case FDIERROR_UNKNOWN_CABINET_VERSION:
        return "Unknown cabinet version";

    case FDIERROR_CORRUPT_CABINET:
        return "Corrupt cabinet";

    case FDIERROR_ALLOC_FAIL:
        return "Memory allocation failed";

    case FDIERROR_BAD_COMPR_TYPE:
        return "Unknown compression type";

    case FDIERROR_MDI_FAIL:
        return "Failure decompressing data";

    case FDIERROR_TARGET_FILE:
        return "Failure writing to target file";

    case FDIERROR_RESERVE_MISMATCH:
        return "Cabinets in set have different RESERVE sizes";

    case FDIERROR_WRONG_CABINET:
        return "Cabinet returned on fdintNEXT_CABINET is incorrect";

    case FDIERROR_USER_ABORT:
        return "User aborted";

    default:
        return "Unknown error";
    }
}

//------------------------------------------------------------------------------
bool CabExtract::Impl::createDirectoryPath(const char* path)
{
    static char cSlash = '\\';

    bool bRetVal = false;

    const int nLength = strlen( path ) + 1;
    char* pszDirectoryPath = (char*)malloc( nLength * sizeof( char ) );
    if (pszDirectoryPath )
    {

        const char* pcszNextDirectory = path;

        //
        // Determine if the path is a UNC path. We do this by looking at the first two bytes
        // and checkin they are both backslashes
        if (nLength > 2 && *pcszNextDirectory == cSlash && *(pcszNextDirectory+1) == cSlash )
        {
            // We need to skip passed this bit and copy it into out local path.
            // "\\Russ\C\"
            pcszNextDirectory += 2;
            while (*pcszNextDirectory && *pcszNextDirectory != cSlash ) pcszNextDirectory++;
            pcszNextDirectory++;
            while (*pcszNextDirectory && *pcszNextDirectory != cSlash ) pcszNextDirectory++;
            strncpy_s(pszDirectoryPath, nLength * sizeof( char ), path, pcszNextDirectory - path );
            pszDirectoryPath[ pcszNextDirectory - path ] = '\000';
        }

        //
        // Set the return value to true because the nly thing that can fail now is the
        // CreateDirectory. If that fails then we change the return value back to fals.
        bRetVal = true;

        //
        // Now, loop over the path, creating directories as we go. If we fail at any point then get out of the loop
        do
        {
            if (*pcszNextDirectory )
                pcszNextDirectory++;

            while (*pcszNextDirectory && *pcszNextDirectory != cSlash )
                pcszNextDirectory++;

            strncpy_s(pszDirectoryPath, nLength * sizeof( char ), path, pcszNextDirectory - path);
            pszDirectoryPath[ pcszNextDirectory - path ] = '\000';

            if (_access( pszDirectoryPath, 0))
            {
                if (!CreateDirectoryA( pszDirectoryPath, NULL ) )
                {
                    bRetVal = false;
                    break;
                }
            }
        }
        while (*pcszNextDirectory );

        free( pszDirectoryPath );
        pszDirectoryPath = NULL;
    } 
    
    return bRetVal;
}
