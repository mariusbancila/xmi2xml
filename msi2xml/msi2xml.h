//------------------------------------------------------------------------------
//
// $Id: msi2xml.h 105 2007-09-27 14:23:27Z dgehriger $
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
#ifndef MSI2XML_H_INCLUDED
#define MSI2XML_H_INCLUDED
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "smrthandle.h"
#include <string>
#include <set>
#include <map>

// define _TCHAR string
typedef std::basic_string<_TCHAR> tstring;

class Msi2Xml
{
public:
    // constructor
    Msi2Xml(int argc, _TCHAR* argv[]);

    // dump file
    void                        dump();

protected:
    // dump summary information
    void                        dumpSummaryInformation();

    // extract cabinets
    void                        extractCabinets();
    
    // dump table column headers
    void                        dumpColumnHeaders(LPCTSTR table, xml::IXMLDOMNode* parentNode);

    // dump table rows
    void                        dumpRows(LPCTSTR table, xml::IXMLDOMNode* parentNode);

    // dump embedded streams
    void                        dumpStreams(xml::IXMLDOMNode* parentNode);

private:
    // parse command line options
    void                        parseCommandLine(int argc, _TCHAR* argv[]);

    // print banner message
    void                        printBanner() const;

    // print usage message
    void                        printUsage() const;

    // get codepage
    long                        codePage() const;

    // write binary stream
    void                        dumpBinaryStream(LPCTSTR id, xml::IXMLDOMElement* elm, MSIHANDLE row, UINT column);

    // indent node
    void                        indent(xml::IXMLDOMNode* parentNode, unsigned level, xml::IXMLDOMNode* before = NULL);

    // callback stub
    static bool __stdcall       extractCallbackStub(void* pv, bool extracted, LPCTSTR entry, size_t size);

    // callback
    bool                        extractCallback(bool extracted, LPCTSTR entry, size_t size);

    // get record string
    tstring                     recordGetString(MSIHANDLE record, UINT col);

    // load text resource
    static std::string          loadTextResource(WORD resourceId);

    // get the module version
    static tstring              moduleVersion();

    // test for valid XML characters
    static bool                 validCharacters(const tstring& str);

private:
    struct FileEntry
    {
        tstring href;
        tstring md5;
    };
    typedef std::map<tstring, FileEntry> Files;

    SmrtMsiHandle               m_db;
    xml::IXMLDOMDocument2Ptr    m_doc;
    xml::IXMLDOMElementPtr      m_rootElement;
    Files                       m_extractedFiles; 
    tstring                     m_inputDir;             // input directory
    tstring                     m_inputPath;            // input file
    tstring                     m_outputDir;            // output directory
    tstring                     m_outputPath;           // output file
    tstring                     m_binDir;               // full path for binary files
    tstring                     m_binDirRel;            // directory for binary files
    tstring                     m_cabDir;               // full path for extracted CAB files
    tstring                     m_cabDirRel;            // directory for cabinet files
    tstring                     m_encoding;             // XML encoding
    tstring                     m_styleSheet;           // name of alternative stylesheet
    int                         m_quiet;                // don't show progress
    bool                        m_nologo;               // don't print banner message
    bool                        m_useDefaultStyleSheet; // use default stylesheet
    bool                        m_dumpStreams;          // write external binary files
    bool                        m_extractCabs;          // extract content of cabinet files
    bool                        m_sortRows;             // sort rows according to first field
    bool                        m_mergeModule;          // extract merge module
    std::set<tstring>           m_streamIds;            // ids of extracted streams
    std::set<tstring>           m_mediaIds;             // ids of decompressed media cabinets
    std::set<tstring>           m_mediasToExtract;
};

#endif // MSI2XML_H_INCLUDED
