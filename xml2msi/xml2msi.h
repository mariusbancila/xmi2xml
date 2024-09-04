//------------------------------------------------------------------------------
//
// $Id: xml2msi.h 94 2007-09-26 13:51:37Z dgehriger $
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
#ifndef XML2MSI_H_INCLUDED
#define XML2MSI_H_INCLUDED
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "CabCompress.h"

class Xml2Msi
{
public:
    // constructor
    Xml2Msi(int argc, _TCHAR* argv[]);

    // destructor
    ~Xml2Msi();

    // create MSI file
    void                        create();

    // update info from command line
    void                        update();

    // validate table
    void                        validateTable(xml::IXMLDOMNode* table);

    // create table
    void                        createTable(xml::IXMLDOMNode* table);

    // populate table
    void                        populateTable(xml::IXMLDOMNode* table);

    // create summary information stream
    void                        createSummaryInfo();

    // create CABs
    void                        buildCabinets();

    // compress files into cabinet
    bool                        compressFiles(LPCTSTR cabinetName, int firstSequence, int lastSequence);

    // check MD5 finger print
    void                        checkMD5(xml::IXMLDOMNode* md5Node, const void* data, int len, int size);

    // get current table
    tstring                     currentTable() const { return m_currentTable; }

    // get current row
    int                         currentRow() const { return m_currentRow; }

    // get current column
    int                         currentColumn() const { return m_currentCol; }

private:
    // resolve hrefs
    tstring                     resolveHref(xml::IXMLDOMNode* hrefNode);

    // build an SQL column specification
    tstring                     buildSQLColSpec(xml::IXMLDOMNode* columnNode);

    // parse command line
    void                        parseCommandLine(int argc, _TCHAR* argv[]);

    // load text resource
    static std::string          loadTextResource(WORD resourceId);

    // get the module version
    static tstring              moduleVersion();

    // resolve a version argument
    static tstring              parseVersion(LPCTSTR argument);

    // print banner
    void                        printBanner() const;

    // print usage
    static void                 printUsage();

    // create a GUID
    static tstring              guid();

    // get node value
    static int                  nodeValue(xml::IXMLDOMNodePtr& node);

private:
    typedef std::map<tstring, tstring> PropertyMap;

    MSIHANDLE                   m_db;
    xml::IXMLDOMDocument2Ptr    m_doc;
    tstring                     m_tempCabDir;   // temporary directory for cabinets
    tstring                     m_tempPath;     // temporary file name (will be deleted upon program exit)
    tstring                     m_hrefPrefix;   // href path prefix
    tstring                     m_currentTable; // current table
    tstring                     m_inputPath;    // input file
    tstring                     m_outputPath;   // output file
    tstring                     m_xmlOutputPath;// output file
    tstring                     m_outputDir;    // output directory
    tstring                     m_packageCode;  // new package code
    tstring                     m_upgradeCode;  // new upgrade code
    tstring                     m_productCode;  // new product code
    bool                        m_componentCode;  // new component code
    tstring                     m_productVersion; // new product version
    tstring                     m_upgradeVersion; // new upgrade version
    PropertyMap                 m_propertyMap;  // property map
    int                         m_currentCol;   // current column
    int                         m_currentRow;   // current row
    int                         m_quiet;        // quiet level
    CabCompress::Compression    m_compression;  // used compression
    bool                        m_nologo;       // don't print banner message
    bool                        m_checkMD5;     // check MD5 checksum
    bool                        m_udpateXml;    // write updated XML
    bool                        m_updateUpgradeVersion;
    bool                        m_mergeModule;  // create MSM merge module
    bool                        m_fixExtension; // need to fix file extension
};

#endif // XML2MSI_H_INCLUDED
