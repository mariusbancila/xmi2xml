//------------------------------------------------------------------
//
// $Id: smrthandle.h 9 2001-11-15 18:24:53Z dgehriger $
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
//------------------------------------------------------------------
//
// Smart handle class
//
// SmrtHandle implements a template class for dealing with all kinds
// of handles. It automatically closes the encapsulated handle
// using the appropriate function. Similar to the auto_ptr class,
// the SmrtHandle class carries an ownership flag, and it transfers 
// ownership if it is assigned to another SmartHandle object.
//
//------------------------------------------------------------------
//
//:Usage
//
// The class takes three template parameters:
//    - the handle type
//    - the return type of the function used to close the handle
//    - the name of the function used to close the handle
//
// For convenience, I recommend using typedefs. Examples are:
//    typedef SmrtHandle<MSIHANDLE, UINT, MsiCloseHandle> SmrtMsiHandle;
//    typedef SmrtHandle<HANDLE,    BOOL, CloseHandle>    SmrtFileHandle;
//    typedef SmrtHandle<HKEY,      LONG, RegCloseKey>    SmrtRegHandle;
//
// Construction:
//
// Pass the raw handle to the constructor. Alternatively, use the
// reference operator to assign a raw handle. 
// Note that you are not allowed to asign a raw handle to SmrtHandle:
//
//  WRONG:  
//    SmrtFileHandle hFile;
//    hFile = CreateFile(...);
//
//  RIGHT:  
//    SmrtFileHandle hFile(CreateFile(...));
//
//  RIGHT:
//    SmrtRegHandle hReg;
//    RegOpenKey(..., &hReg);
//
//  RIGHT:
//    SmrtFileHandle hFile1(CreateFile(...));
//    SmrtFileHandle hFile2;
//    ...
//    hFile2 = hFile1; // assignment between SmartHandles allowed
//
//------------------------------------------------------------------

#ifndef SMRT_HANDLE_INCLUDED
#define SMRT_HANDLE_INCLUDED
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

template <class HNDL, class RET, RET (WINAPI* closeHndl)(HNDL)>
class SmrtHandle
{
public:
  typedef HNDL* PHNDL;

  // constructor
  explicit SmrtHandle(const HNDL& hndl = NULL) 
    : m_owns(true), m_hndl(hndl) {};

  // copy constructor
  explicit SmrtHandle(const SmrtHandle& h) 
    : m_owns(h.m_owns), m_hndl(h.release()) {};

  // destructor
  ~SmrtHandle() 
    { if (m_owns && m_hndl) closeHndl(m_hndl); }

  // assignment to smart handle
  SmrtHandle& operator=(const SmrtHandle& h) 
    { if (this != &h) 
      { if (m_hndl != h.m_hndl)
        { if (m_owns && m_hndl) closeHndl(m_hndl);
          m_owns = h.m_owns; 
        }
        else if (h.m_owns)
          m_owns = true;
        m_hndl = h.release(); 
      }
      return (*this); 
    }

  // reference operator (destroys enclosed handle)
  PHNDL operator&()
    { if (m_owns && m_hndl) closeHndl(m_hndl);
      m_owns = true;
      return &m_hndl; 
    }

  // return encaplsulated handle
  operator const HNDL() const 
    { return m_hndl; }

  // comparisson
  bool operator==(const HNDL& hndl) const
    { return m_hndl == hndl; }

  // comparisson
  bool operator!=(const HNDL& hndl) const
    { return m_hndl != hndl; }

  // release and return encapsulated handle
  HNDL release() const
    { m_owns = false; return m_hndl; }

  // check if handle is NULL
  bool isNull() const
    { return m_hndl == NULL; }

private:
    mutable bool  m_owns;
    HNDL          m_hndl;
};

#endif // SMRT_HANDLE_INCLUDED

