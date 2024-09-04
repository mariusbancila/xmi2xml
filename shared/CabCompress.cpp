//------------------------------------------------------------------------------
//
// $Id: CabCompress.cpp 94 2007-09-26 13:51:37Z dgehriger $
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
#include "CabCompress.h"
#include <windows.h>
#include <fci.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
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
struct CabCompress::Impl
{
    HFCI                hfci;
    ERF                 erf;
    int                 cabIndex;
    string              cabTemplate;

    static bool         createDirectoryPath(const char* path);
    static const char*  fcierrorToString(int err);

    static FNFCIGETNEXTCABINET(getNextCab);
    static FNFCIFILEPLACED(filePlaced);
    static FNFCISTATUS(progress);
    static FNFCIGETOPENINFO(getOpenInfo);
    static FNFCIALLOC(alloc);
    static FNFCIFREE(free);
    static FNFCIOPEN(open);
    static FNFCIREAD(read);
    static FNFCIWRITE(write);
    static FNFCICLOSE(close);
    static FNFCISEEK(seek);
    static FNFCIDELETE(remove);
    static FNFCIGETTEMPFILE(getTempFile);
};

//------------------------------------------------------------------------------
CabCompress::CabCompress(const _TCHAR*  cabFolder, 
                         const _TCHAR*  cabTemplate, 
                         unsigned long  mediaSize, 
                         int            cabIndexStart /* = 1 */, 
                         unsigned short setID /* = 0 */, 
                         const _TCHAR*  diskName /* = _T("") */, 
                         unsigned int   headerReserved /* = 0 */) :
    m_pImpl(new Impl)
{
    // initialize members
    m_pImpl->hfci = NULL;
    m_pImpl->cabTemplate = cabTemplate ?  ATL::CT2A(cabTemplate) : "";
    m_pImpl->cabIndex = cabIndexStart;

    CCAB ccab = { 0 };
    ccab.cb                 = mediaSize == 0 ? ULONG_MAX : mediaSize;
    ccab.cbFolderThresh     = ULONG_MAX;
    ccab.iCab               = m_pImpl->cabIndex;
    ccab.setID              = setID;
    ccab.iDisk              = 0;
    ccab.cbReserveCFHeader  = headerReserved;
    strcpy_s(ccab.szDisk, ARRAYSIZE(ccab.szDisk), diskName ? ATL::CT2A(diskName) : "");
    strcpy_s(ccab.szCabPath, ARRAYSIZE(ccab.szCabPath), cabFolder ?  ATL::CT2A(cabFolder) : "");
    sprintf_s(ccab.szCab, m_pImpl->cabTemplate.c_str() , ccab.iCab);

    // try creating error context
    m_pImpl->hfci = 
        FCICreate(&m_pImpl->erf,
                  Impl::filePlaced,
                  Impl::alloc, 
                  Impl::free, 
                  Impl::open, 
                  Impl::read, 
                  Impl::write, 
                  Impl::close, 
                  Impl::seek,
                  Impl::remove,
                  Impl::getTempFile,
                  &ccab,
                  m_pImpl);

    if (m_pImpl->hfci == NULL)
        throw runtime_error(Impl::fcierrorToString(m_pImpl->erf.erfOper));
}

//------------------------------------------------------------------------------
CabCompress::~CabCompress()
{
    if (m_pImpl->hfci)
    {
        flush();
        FCIDestroy(m_pImpl->hfci);
    }

    delete m_pImpl;
}

//------------------------------------------------------------------------------
std::string CabCompress::evalCabTemplate(int index) const
{
    char cab[MAX_PATH];
    sprintf_s(cab, m_pImpl->cabTemplate.c_str() , index);
    return cab;
}

//------------------------------------------------------------------------------
int CabCompress::addFile(const _TCHAR*  filePath, 
                         const _TCHAR*  fileName /* = 0 */, 
                         Compression    comp /* = compMSZIP */)
{
    ATL::CT2A filePathA(filePath);

    char cabName[_MAX_FNAME];
    if (fileName == 0)
    {
        char drive[_MAX_DRIVE];
        char dir[_MAX_DIR];
        char fname[_MAX_FNAME];
        char ext[_MAX_EXT];
        _splitpath_s(filePathA, 
                     drive, ARRAYSIZE(drive), 
                     dir, ARRAYSIZE(dir), 
                     fname, ARRAYSIZE(fname), 
                     ext, ARRAYSIZE(ext));
        _makepath_s(cabName, ARRAYSIZE(cabName), NULL, NULL, fname, ext);
    }
    else
    {
        strcpy_s(cabName, ARRAYSIZE(cabName), ATL::CT2A(fileName));
    }

    TCOMP ct;
    switch (comp)
    {
    case compNone:      ct = tcompTYPE_NONE; break;
    case compMSZIP:     ct = tcompTYPE_MSZIP; break;
    case compLZX:       ct = tcompTYPE_LZX | tcompLZX_WINDOW_HI; break;
    case compQuantum:   ct = tcompTYPE_QUANTUM | tcompQUANTUM_LEVEL_HI | tcompQUANTUM_MEM_HI; break;
    default:            ct = tcompTYPE_NONE; break;
    }

    int index = m_pImpl->cabIndex;
    BOOL res = FCIAddFile(m_pImpl->hfci, static_cast<char*>(filePathA), cabName, FALSE, Impl::getNextCab, Impl::progress, Impl::getOpenInfo, ct);
    return (res ? index : -1);
}

//------------------------------------------------------------------------------
int CabCompress::flushCabinet()
{
    BOOL res = FCIFlushCabinet(m_pImpl->hfci, FALSE, Impl::getNextCab, Impl::progress);
    return (res ? m_pImpl->cabIndex : -1);
}

//------------------------------------------------------------------------------
int CabCompress::flush()
{
    BOOL res = FCIFlushFolder(m_pImpl->hfci, Impl::getNextCab, Impl::progress);
    return (res ? m_pImpl->cabIndex : -1);
}

//------------------------------------------------------------------------------
FNFCIGETNEXTCABINET(CabCompress::Impl::getNextCab)
{
    Impl* pImpl = reinterpret_cast<Impl*>(pv);

    // create cabinet name
    sprintf_s(pccab->szCab, pImpl->cabTemplate.c_str(), pccab->iCab);
    return TRUE;
}

//------------------------------------------------------------------------------
FNFCISTATUS(CabCompress::Impl::progress)
{
    return 0;
}

//------------------------------------------------------------------------------
FNFCIGETOPENINFO(CabCompress::Impl::getOpenInfo)
{
    BY_HANDLE_FILE_INFORMATION	finfo;
    FILETIME filetime;
    HANDLE handle;
    DWORD attrs;
    int hf;

    /*
    * Need a Win32 type handle to get file date/time
    * using the Win32 APIs, even though the handle we
    * will be returning is of the type compatible with
    * _open
    */
    handle = CreateFileA(pszName,
                         GENERIC_READ,
                         FILE_SHARE_READ,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                         NULL);

    if (handle == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    if (GetFileInformationByHandle(handle, &finfo) == FALSE)
    {
        CloseHandle(handle);
        return -1;
    }

    FileTimeToLocalFileTime(
        &finfo.ftLastWriteTime, 
        &filetime
        );

    FileTimeToDosDateTime(
        &filetime,
        pdate,
        ptime
        );

    attrs = GetFileAttributesA(pszName);
    *pattribs = 0;

    if (attrs != INVALID_FILE_ATTRIBUTES)
    {
        if (attrs & FILE_ATTRIBUTE_READONLY)    *pattribs |= _A_RDONLY;
        if (attrs & FILE_ATTRIBUTE_SYSTEM)      *pattribs |= _A_SYSTEM;
        if (attrs & FILE_ATTRIBUTE_HIDDEN)      *pattribs |= _A_HIDDEN;
        if (attrs & FILE_ATTRIBUTE_ARCHIVE)     *pattribs |= _A_ARCH;
    }

    CloseHandle(handle);


    /*
    * Return handle using _open
    */
    _sopen_s(&hf, pszName, _O_RDONLY | _O_BINARY, _SH_DENYNO, _S_IREAD);
    return hf;
}

//------------------------------------------------------------------------------
FNFCIFILEPLACED(CabCompress::Impl::filePlaced)
{
    Impl* pImpl = reinterpret_cast<Impl*>(pv);

    // store index
    pImpl->cabIndex =  pccab->iCab;

    return 0;
}

//------------------------------------------------------------------------------
FNFCIALLOC(CabCompress::Impl::alloc)
{
    return ::malloc(cb);
}

//------------------------------------------------------------------------------
FNFCIFREE(CabCompress::Impl::free)
{
    ::free(memory);
}

//------------------------------------------------------------------------------
FNFCIOPEN(CabCompress::Impl::open)
{
    int result;
    *err = ::_sopen_s(&result, pszFile, oflag, _SH_DENYNO, pmode);
    return result;
}

//------------------------------------------------------------------------------
FNFCIREAD(CabCompress::Impl::read)
{
    unsigned int result = (unsigned int) ::_read(hf, memory, cb);
    if (result == -1)
        *err = errno;
    return result;
}

//------------------------------------------------------------------------------
FNFCIWRITE(CabCompress::Impl::write)
{
    unsigned int result = (unsigned int) ::_write(hf, memory, cb);
    if (result == -1)
        *err = errno;
    return result;
}

//------------------------------------------------------------------------------
FNFCICLOSE(CabCompress::Impl::close)
{
    int result = ::_close(hf);
    if (result != 0)
        *err = errno;
    return result;
}

//------------------------------------------------------------------------------
FNFCISEEK(CabCompress::Impl::seek)
{
    long result = ::_lseek(hf, dist, seektype);
    if (result == -1)
        *err = errno;
    return result;
}

//------------------------------------------------------------------------------
FNFCIDELETE(CabCompress::Impl::remove)
{
    int result = ::remove(pszFile);
    if (result != 0)
        *err = errno;
    return result;
}

//------------------------------------------------------------------------------
FNFCIGETTEMPFILE(CabCompress::Impl::getTempFile)
{
    char* tmp = _tempnam("","cab");      // Get a name
    if ((tmp != NULL) && (strlen(tmp) < (size_t)cbTempName)) 
    {
        strcpy_s(pszTempName, cbTempName, tmp); // Copy to caller's buffer
        free(tmp);                      // Free temporary name buffer
        return TRUE;                    // Success
    }

    if (tmp)
    {
        free(tmp);
    }

    return FALSE;
}

//------------------------------------------------------------------------------
const char* CabCompress::Impl::fcierrorToString(int err)
{
    switch (err)
    {
    case FCIERR_NONE:           return "No error";
    case FCIERR_OPEN_SRC:       return "Failure opening file to be stored in cabinet";
    case FCIERR_READ_SRC:       return "Failure reading file to be stored in cabinet";
    case FCIERR_ALLOC_FAIL:     return "Insufficient memory in FCI";
    case FCIERR_TEMP_FILE:      return "Could not create a temporary file";
    case FCIERR_BAD_COMPR_TYPE: return "Unknown compression type";
    case FCIERR_CAB_FILE:       return "Could not create cabinet file";
    case FCIERR_USER_ABORT:     return "Client requested abort";
    case FCIERR_MCI_FAIL:       return "Failure compressing data";
    default:                    return "Unknown error";
    }
}
