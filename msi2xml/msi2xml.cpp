//------------------------------------------------------------------------------
//
// $Id: msi2xml.cpp 107 2007-12-06 22:17:36Z dgehriger $
//
// Copyright (c) 2001-2005 Daniel Gehriger <gehriger at linkcad dot com>
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
#include "msi2xml.h"
#include "resource.h"
#include "md5.h"
#include "base64.h"
#include "getopt.h"
#include "CabExtract.h"
#include "consolecolor.h"

//------------------------------------------------------------------------------
// Binary stream chunk size and corresponding size of base64 encoded data
//------------------------------------------------------------------------------
#define CHUNK_BIN     54
#define CHUNK_BASE64  (4 * CHUNK_BIN / 3 + 1)

//------------------------------------------------------------------------------
// global variables
//------------------------------------------------------------------------------
#define BUF_SIZE 512                 // initial buffer size (arbitrary)

//------------------------------------------------------------------------------
// Main entry point
//------------------------------------------------------------------------------
int __cdecl _tmain(int argc, _TCHAR* argv[], _TCHAR* envp[])
{
    int exitCode = 0;

    // Initialize COM
    CoInitialize(NULL);

    try 
    {
        // create Msi2Xml instance
        Msi2Xml msi2xml(argc, argv);

        // dump it
        msi2xml.dump();
    }
    catch (const _com_error& e) 
    {
        if (e.Error() != E_FAIL) 
        {
            tcerr << color::red << _T("Error: ") << e.ErrorMessage() 
                << color::base << std::endl;
        }
        else 
        {
            tcerr << color::red << _T("Error: Failed to convert the MSI database!") 
                << color::base << std::endl;
        }

        exitCode = 1;
    }
    catch (const std::runtime_error& e)
    {
        if (e.what() == "")
        {
            tcerr << color::red << _T("Failed to convert the MSI database!") << color::base << std::endl;
        }
        else
        {
            std::cerr << color::red << e.what() << color::base << std::endl;
        }
    }

    CoUninitialize( );
    return exitCode;
}

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
Msi2Xml::Msi2Xml(int argc, _TCHAR* argv[]) :
    m_encoding(_T("US-ASCII")),
    m_useDefaultStyleSheet(true),
    m_dumpStreams(false),
    m_extractCabs(false),
    m_sortRows(true),
    m_mergeModule(false),
    m_quiet(false),
    m_nologo(false)
{
    HRESULT hr = m_doc.CreateInstance(__uuidof(xml::DOMDocument60));
    if (FAILED(hr))
    {
        tcerr << color::red 
            << _T("Error: You need to install MSXML 6.0 or later.\n\n")
            << _T("       Please visit the following link to download the setup file:") << std::endl
            << _T("       <http://tinyurl.com/2um5v7>\n") 
            << color::base << std::endl;

        _com_issue_error(hr);
    }

    // parse command line
    parseCommandLine(argc, argv);
}

//------------------------------------------------------------------------------
// Dump
//------------------------------------------------------------------------------
void Msi2Xml::dump()
{
    // Load template
    m_doc->validateOnParse = VARIANT_FALSE;
    m_doc->setProperty(L"ProhibitDTD", VARIANT_FALSE);
    m_doc->loadXML(loadTextResource(IDR_TEMPLATE_DT_XML).c_str());

    assert(m_doc->hasChildNodes());
    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^
    // This fails if you removed the above loadXML() code bit.
    // The code below relies on a default XML tree being available :
    //
    // <?xml version="1.0" standalone="yes"?>
    // <?xml:stylesheet type="text/xsl" href="msi.xsl" ?>
    // <msi version="2.0">
    // </msi>

    xml::IXMLDOMNodeListPtr pNodeList(m_doc->childNodes);

    // replace processing instructions
    { 
        xml::IXMLDOMNodePtr pPiOld(pNodeList->nextNode());  
        tostringstream tssPi;
        tssPi << _T("version=\"1.0\" ")
              << _T("encoding=\"") << m_encoding 
              << _T("\"  ") << _T("standalone=\"yes\"");
        xml::IXMLDOMProcessingInstructionPtr pPiNew(
            m_doc->createProcessingInstruction(L"xml", tssPi.str().c_str()));
        m_doc->replaceChild(pPiNew, pPiOld);
    }

    // add / remove stylesheet
    { 
        xml::IXMLDOMNodePtr pPiOld(pNodeList->nextNode());  
        if (!m_useDefaultStyleSheet) 
        {
            if (!m_styleSheet.empty()) 
            {
                tostringstream tssSs;
                tssSs << _T("type=\"text/xsl\" href=\"") << m_styleSheet << _T("\"");
                xml::IXMLDOMProcessingInstructionPtr pPiNew(
                    m_doc->createProcessingInstruction(L"xml:stylesheet", tssSs.str().c_str()));
                m_doc->replaceChild(pPiNew, pPiOld);
            }
            else 
            {
                m_doc->removeChild(pPiOld);
            }
        }
    }

    // get root node
    m_rootElement = m_doc->getElementsByTagName(L"msi")->nextNode();

    // open MSI database
    OK(MsiOpenDatabase(m_inputPath.c_str(), MSIDBOPEN_READONLY, &m_db));

    // set codepage attribute
    if (long codepage = codePage()) 
    {
        m_rootElement->setAttribute(L"codepage", _variant_t(codepage));
    }

    // set msm attribute
    if (m_mergeModule)
    {
        m_rootElement->setAttribute(L"msm", L"yes");
    }

    // dump summary information stream
    dumpSummaryInformation();

    // extract files from cabs
    if (m_extractCabs)
    {
        extractCabinets();
    }

    // obtain and sort table names
    typedef std::list<tstring> Tables;
    Tables tables;
    SmrtMsiHandle hViewTables, hRec;
    UINT res;
    OK(MsiDatabaseOpenView(m_db, _T("SELECT `Name` FROM `_Tables`"), &hViewTables));
    OK(MsiViewExecute(hViewTables, NULL));

    // iterate over tables
    while ((res = MsiViewFetch(hViewTables, &hRec)) != ERROR_NO_MORE_ITEMS) 
    {
        OK(res);

        // retrieve table name
        tables.push_back(recordGetString(hRec, 1));
    }

    OK(MsiViewClose(hViewTables));
    tables.sort();

    // dump tables
    for (Tables::iterator it = tables.begin(); it != tables.end(); ++it) 
    {
        if (!m_quiet) 
        {
            tcerr << _T("Writing table '") << *it << _T("'") << std::endl;
        }

        // emit table element
        indent(m_rootElement, 1);
        xml::IXMLDOMElementPtr pElement(m_doc->createElement(L"table"));
        pElement->setAttribute(L"name", it->c_str());
        xml::IXMLDOMNodePtr parentNode(m_rootElement->appendChild(pElement));

        try
        {
            // list column headers
            dumpColumnHeaders(it->c_str(), parentNode);

            // list rows
            dumpRows(it->c_str(), parentNode);
        }
        catch (...)
        {
            tcerr << color::red << _T("Error: Exception while dumping table '")
                  << *it << _T("'")
                  << color::base << std::endl;

            m_rootElement->removeChild(parentNode);
        }
        
        indent(parentNode, 1);
        indent(m_rootElement, 0);
    }

    // dump _Streams pseudo table
    indent(m_rootElement, 1);
    xml::IXMLDOMElementPtr pElement(m_doc->createElement(L"table"));
    pElement->setAttribute(L"name", L"_Streams");
    xml::IXMLDOMNodePtr parentNode(m_rootElement->appendChild(pElement));

    dumpStreams(parentNode);
    indent(parentNode, 1);
    indent(m_rootElement, 0);

    // validate generated XML against DTD
    xml::IXMLDOMParseErrorPtr err = m_doc->validate();
    if (err->errorCode != 0) 
    {
        tcerr << color::red << _T("Error ") << _T(":\n") 
              << (LPCTSTR)err->reason << color::base ;
    }

    // save document
    OK(m_doc->save(m_outputPath.c_str()));
}

//------------------------------------------------------------------------------
// Dump summary information
//------------------------------------------------------------------------------
void Msi2Xml::dumpSummaryInformation()
{
    // summary information tags
    static LPCTSTR szSumInfoTags[19] = 
    {
        _T("codepage"), 
        _T("title"), 
        _T("subject"),
        _T("author"), 
        _T("keywords"), 
        _T("comments"),
        _T("template"), 
        _T("lastauthor"), 
        _T("revnumber"),
        _T("__unused__"), 
        _T("lastprinted"), 
        _T("createdtm"),
        _T("lastsavedtm"), 
        _T("pagecount"), 
        _T("wordcount"),
        _T("charcount"), 
        _T("__unused__"), 
        _T("appname"),
        _T("security")
    };

    // dump summary stream
    indent(m_rootElement, 1);
    indent(m_rootElement, 1);
    xml::IXMLDOMElementPtr pElement(m_doc->createElement(L"summary"));
    xml::IXMLDOMNodePtr pNodeSumInfo(m_rootElement->appendChild(pElement));

    SmrtMsiHandle hSumInfo;
    OK(MsiGetSummaryInformation(m_db, 0, 0, &hSumInfo));

    tcharvector buf(BUF_SIZE);
    DWORD len;

    for (UINT i = 0; i < 19; ++i) 
    {
        if (_tcscmp(szSumInfoTags[i], _T("__unused__")) == 0)
            continue; // unused property IDs

        FILETIME ftValue;
        UINT uiDataType;
        INT iValue;
        xml::IXMLDOMElementPtr pElement;
        pElement = m_doc->createElement(szSumInfoTags[i]);

        UINT res;
        while ((res = MsiSummaryInfoGetProperty(hSumInfo, i+1, 
            &uiDataType, &iValue, &ftValue, &buf[0], &(len=buf.size()))) == ERROR_MORE_DATA) 
        {
            buf.resize(len+1); OK(res);
        }

        switch (uiDataType) 
        {
        case VT_I2:        // 2 byte signed int
        case VT_I4: {      // 4 byte signed int
            tostringstream oss;
            oss << iValue;
            pElement->text = oss.str().c_str();
            break; }

        case VT_LPSTR:     // null terminated string
            pElement->text = &buf[0];
            break;

        case VT_FILETIME: { // FILETIME
            FILETIME ftLocal;
            SYSTEMTIME systime;
            FileTimeToLocalFileTime(&ftValue, &ftLocal);
            FileTimeToSystemTime(&ftLocal, &systime);
            tostringstream oss;
            oss << std::setfill(_T('0'))
                << std::setw(2) << static_cast<int>(systime.wMonth)  << _T("/")
                << std::setw(2) << static_cast<int>(systime.wDay)    << _T("/")
                << std::setw(4) << static_cast<int>(systime.wYear)   << _T(" ")
                << std::setw(2) << static_cast<int>(systime.wHour)   << _T(":")
                << std::setw(2) << static_cast<int>(systime.wMinute);
            pElement->text = oss.str().c_str();
            break; }

        case VT_EMPTY:
            break;

        default:
            _com_issue_error(ERROR_UNKNOWN_PROPERTY);
        }

        indent(pNodeSumInfo, 2);
        pNodeSumInfo->appendChild(pElement);
    }

    indent(pNodeSumInfo, 1);
    indent(m_rootElement, 1);
}

//------------------------------------------------------------------------------
// Extract cabinet files
//------------------------------------------------------------------------------
void Msi2Xml::extractCabinets()
{
    // prepare a temporary folder for internal CABs
    _TCHAR tmpDir[MAX_PATH];
    _TCHAR tmpDirDir[MAX_PATH];
    GetTempPath(MAX_PATH, tmpDir);
    GetTempFileName(tmpDir, _T("cab"), 0, tmpDirDir);
    DeleteFile(tmpDirDir);
    CreateDirectory(tmpDirDir, NULL);
    _tcscat_s(tmpDirDir, ARRAYSIZE(tmpDirDir), _T("\\"));

    // list of embedded cabinets
    typedef std::list<tstring> Cabs;
    Cabs cabs;
    if (!m_mergeModule)
    {
        // list the rows
        SmrtMsiHandle hViewMedia;
        OK(MsiDatabaseOpenView(m_db, _T("SELECT `Cabinet` FROM `Media`"), &hViewMedia));
        OK(MsiViewExecute(hViewMedia, NULL));

        // get all cab names from Media table
        UINT res;
        SmrtMsiHandle hRec;
        while ((res = MsiViewFetch(hViewMedia, &hRec)) != ERROR_NO_MORE_ITEMS) 
        {
            tstring cab = recordGetString(hRec, 1);

            // skip unless in media list
            if (!m_mediasToExtract.empty() 
                && m_mediasToExtract.find(cab) == m_mediasToExtract.end())
                continue;

            if (!cab.empty())
            {
                cabs.push_back(cab);
            }
        }
    }
    else
    {
        cabs.push_back(_T("#MergeModule.CABinet"));
    }

    // extract all internal cabs
    Cabs::iterator it = cabs.begin();
    while (it != cabs.end())
    {
        // dump embedded cab stream to temporary file
        bool internal = (it->at(0) == _T('#'));
        if (internal)
        {
            tstring cabName = it->substr(1);
            tstring cabPath = tmpDirDir + cabName;

            tstring query = _T("SELECT `Data` FROM `_Streams` WHERE `Name`='") 
                            + cabName + _T("'");
            SmrtMsiHandle hViewStream;
            OK(MsiDatabaseOpenView(m_db, query.c_str(), &hViewStream));
            OK(MsiViewExecute(hViewStream, NULL));

            // fetch the stream
            SmrtMsiHandle hRecStream;
            if (MsiViewFetch(hViewStream, &hRecStream) == ERROR_NO_MORE_ITEMS)
            {
                tcerr << color::yellow << _T("Warning: missing embedded stream '") 
                    << cabName << _T("'") << color::base << std::endl;
                it = cabs.erase(it);
                continue;
            }

            // create file
            SmrtFileHandle hFile(CreateFile(cabPath.c_str(),
               GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));

            if (hFile.isNull())
               _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

            // read binary data
            DWORD msiRecordSize = MsiRecordDataSize(hRecStream, 1);

            // 1MB buffer
            std::vector<char> bufBinary(1024 * 1024);
            DWORD cbSize = static_cast<DWORD>(bufBinary.size());
            DWORD totalSize = 0;
            do
            {
               OK(MsiRecordReadStream(hRecStream, 1, &bufBinary[0], &cbSize));

               totalSize += cbSize;

               // write file
               DWORD dwWritten = 0;
               WriteFile(hFile, (LPCVOID)&bufBinary[0], cbSize, &dwWritten, NULL);
            }
            while (cbSize == static_cast<DWORD>(bufBinary.size()));

            // close the file handle
            CloseHandle(hFile.release());

            if (msiRecordSize != totalSize)
            {
               tcerr << color::red << _T("Error: Failed to read the MSI record!") << color::base << std::endl;

               _com_issue_error(ERROR_READ_FAULT);
            }
        }
        ++it;
    }

    // decompress
    for (Cabs::const_iterator it = cabs.begin(); it != cabs.end(); ++it)
    {
        bool internal = (it->at(0) == _T('#'));
        tstring cabName = (internal ? it->substr(1) : *it);
        tstring cabPath = (internal ? tmpDirDir : m_inputDir) + cabName;

        // extract files from cab
        CabExtract cabex(cabPath.c_str());
        if (!cabex.extractTo(m_cabDir.c_str(), extractCallbackStub, this))
            _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

        // store cab index
        m_mediaIds.insert(cabName);

        // save Id so that cab does not get extracted a second time
        if (internal)
        {
            m_streamIds.insert(cabName);
        }
    }
   
    // delete temporary cabinet files
    for (Cabs::const_iterator it = cabs.begin(); it != cabs.end(); ++it)
    {
        bool internal = (it->at(0) == _T('#'));
        if (internal)
        {
            tstring cabName = it->substr(1);
            tstring cabPath = tmpDirDir + cabName;
            DeleteFile(cabPath.c_str());
        }
    }
    RemoveDirectory(tmpDirDir);

}

//------------------------------------------------------------------------------
// Cab extraction callback
//------------------------------------------------------------------------------
bool Msi2Xml::extractCallbackStub(void* pv, bool extracted, LPCTSTR entry, size_t size)
{
    Msi2Xml* pThis = reinterpret_cast<Msi2Xml*>(pv);

    return pThis->extractCallback(extracted, entry, size);
}

//------------------------------------------------------------------------------
bool Msi2Xml::extractCallback(bool extracted, LPCTSTR entry, size_t size)
{
    if (extracted)
    {
        // create full path
        tstring cabPath = m_cabDir + entry;
        tstring href = m_cabDirRel + _T('/') + entry;

        if (!m_quiet) 
        {
            tcerr << _T("extracting '") << entry << _T("'") << std::endl;
        }

        // calculate MD5
        MD5_CTX ctx;
        MD5Init(&ctx);
        SmrtFileHandle hFile(CreateFile(cabPath.c_str(), 
            GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL));

        if (hFile == INVALID_HANDLE_VALUE) 
            _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

        if (GetFileSize(hFile, NULL) > 0)
        {
            SmrtFileHandle hMap(CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL));
            if (hMap == NULL) 
                _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
            SmrtFileMap pMap(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0));
            MD5Update(&ctx, (LPCVOID)pMap, GetFileSize(hFile, NULL));
        }
        MD5Final(&ctx);

        tostringstream oss;
        oss.fill(_T('0'));
        oss.setf(std::ios::hex, std::ios::basefield);
        for (int j = 0; j < 16; ++j) 
        {
            oss << std::setw(2) << static_cast<unsigned int>(ctx.digest[j]);
        }

        // add to file map
        FileEntry fe;
        fe.href = href.c_str();
        fe.md5  = oss.str();
        m_extractedFiles[entry] = fe;
    }

    return true;
}

//------------------------------------------------------------------------------
// List column headers
//------------------------------------------------------------------------------
void Msi2Xml::dumpColumnHeaders(LPCTSTR table, xml::IXMLDOMNode* parentNode)
{
    // obtain primary keys
    typedef std::set<tstring> StrSet;
    StrSet keys;

    if (_tcscmp(table, _T("_Streams")) != 0) 
    {
        SmrtMsiHandle hRecKeys;
        OK(MsiDatabaseGetPrimaryKeys(m_db, table, &hRecKeys));
        unsigned fieldCount = MsiRecordGetFieldCount(hRecKeys);
        if (fieldCount == 0xffffffff)
            return;

        for (UINT i = 1; i <= fieldCount; ++i) 
        {
            tstring name = recordGetString(hRecKeys, i);
            keys.insert(name);
        }

        // list the columns
        SmrtMsiHandle hViewCols;
        {
            tostringstream oss;
            oss << _T("SELECT Number,Name FROM _Columns WHERE `Table`='") 
                << table << _T("' ORDER BY `Number`");
            OK(MsiDatabaseOpenView(m_db, oss.str().c_str(), &hViewCols));
            OK(MsiViewExecute(hViewCols, NULL));
        }

        // determine column types
        SmrtMsiHandle hViewRows;
        {
            tostringstream oss;
            oss << _T("SELECT * FROM `") << table << _T("`");
            OK(MsiDatabaseOpenView(m_db, oss.str().c_str(), &hViewRows));
            OK(MsiViewExecute(hViewRows, NULL));
        }

        SmrtMsiHandle hRecInfo;
        OK(MsiViewGetColumnInfo(hViewRows, MSICOLINFO_TYPES, &hRecInfo));
        OK(MsiViewClose(hViewRows));

        // iterate over columns
        SmrtMsiHandle hRec;
        UINT res;
        while ((res = MsiViewFetch(hViewCols, &hRec)) != ERROR_NO_MORE_ITEMS) 
        {
            OK(res);
            // create col element
            indent(parentNode, 2);
            xml::IXMLDOMElementPtr pElement;
            pElement = m_doc->createElement(L"col");

            // emit name
            tstring name = recordGetString(hRec, 2);
            pElement->appendChild(m_doc->createTextNode(name.c_str()));

            // emit key attribute
            if (keys.find(name) != keys.end()) 
            {
                pElement->setAttribute(L"key", L"yes");
            }

            // emit column format
            UINT col = MsiRecordGetInteger(hRec, 1);
            tstring format = recordGetString(hRecInfo, col);
            pElement->setAttribute(L"def", format.c_str());
            parentNode->appendChild(pElement);
        }

        OK(MsiViewClose(hViewCols));
    }
    else 
    {
        // emit "Name"
        indent(parentNode, 2);
        xml::IXMLDOMElementPtr pElementName(m_doc->createElement(L"col"));
        pElementName->appendChild(m_doc->createTextNode(L"Name"));
        pElementName->setAttribute(L"key", L"yes");
        pElementName->setAttribute(L"def", L"s62");
        parentNode->appendChild(pElementName);

        // emit "Data"
        indent(parentNode, 2);
        xml::IXMLDOMElementPtr pElementData(m_doc->createElement(L"col"));
        pElementData->appendChild(m_doc->createTextNode(L"Data"));
        pElementData->setAttribute(L"def", L"V0");
        parentNode->appendChild(pElementData);
    }
}

//------------------------------------------------------------------------------
// List rows
//------------------------------------------------------------------------------
void Msi2Xml::dumpRows(LPCTSTR table, xml::IXMLDOMNode* parentNode)
{
    bool isFileTable = (_tcscmp(table, _T("File")) == 0);

    // list the rows
    SmrtMsiHandle hViewRows;
    {
        tostringstream oss;
        oss << _T("SELECT * FROM `") << table << _T("`");
        OK(MsiDatabaseOpenView(m_db, oss.str().c_str(), &hViewRows));
        OK(MsiViewExecute(hViewRows, NULL));
    }

    // determine column type
    SmrtMsiHandle hRecInfo;
    OK(MsiViewGetColumnInfo(hViewRows, MSICOLINFO_TYPES, &hRecInfo));

    // determine key columns
    SmrtMsiHandle hRecKeys;
    OK(MsiDatabaseGetPrimaryKeys(m_db, table, &hRecKeys));
    int nKeyMax = MsiRecordGetFieldCount(hRecKeys);

    // prepare key container
    typedef std::deque<tstring> Idx;
    Idx idx;

    // iterate over rows
    SmrtMsiHandle hRecRow;
    UINT res;
    while ((res = MsiViewFetch(hViewRows, &hRecRow)) != ERROR_NO_MORE_ITEMS) 
    {
        // build up key string used for indexing
        tostringstream tssIdx;
        for (int i = 0; i < nKeyMax; ++i) 
        {
            tssIdx << _T(".") << recordGetString(hRecRow, i+1);
        }

        // search insert location
        Idx::iterator it = std::lower_bound(idx.begin(), idx.end(), tssIdx.str());

        // retrieve node at insert location
        tostringstream oss;
        oss << _T("row[") << std::distance(idx.begin(), it)+1 << _T("]");
        xml::IXMLDOMNodePtr pInTd(parentNode->selectSingleNode(oss.str().c_str()));

        // insert new index
        idx.insert(it, tssIdx.str());

        // get pointer to row at insert location for new row
        xml::IXMLDOMNodePtr pInRow(pInTd ? pInTd->previousSibling : NULL);

        // create and add "row" element
        indent(parentNode, 2, pInRow);
        xml::IXMLDOMElementPtr pElementRow(m_doc->createElement(L"row"));
        xml::IXMLDOMNodePtr pNodeRow(parentNode->insertBefore(pElementRow, 
            _variant_t(pInRow, pInRow != NULL)));

        // build row record
        UINT fieldCount = MsiRecordGetFieldCount(hRecRow);
        for (UINT col = 1; col <= fieldCount; ++col) 
        {
            // create and add "td" element
            indent(pNodeRow, 3);
            xml::IXMLDOMElementPtr pElementTd(m_doc->createElement(L"td"));
            xml::IXMLDOMNodePtr pNodeTd(pNodeRow->appendChild(pElementTd));

            // special handling for File table
            if (isFileTable && col == 1)
            {
                tstring fileName = recordGetString(hRecRow, col);
                Files::const_iterator it = m_extractedFiles.find(fileName);
                if (it != m_extractedFiles.end())
                {
                    pElementTd->setAttribute(L"href", it->second.href.c_str());
                    pElementTd->setAttribute(L"md5", it->second.md5.c_str());
                }
            }

            if (MsiRecordIsNull(hRecRow, col))
                continue;

            // determine column type
            tstring type = recordGetString(hRecInfo, col);
            switch (tolower(type.at(0))) 
            {
            case 's':      // string type
            case 'l':      // localizable string
            case 'i': {    // integer
                tstring fieldContent = recordGetString(hRecRow, col);
                if (validCharacters(fieldContent))
                {
                    xml::IXMLDOMTextPtr pT(m_doc->createTextNode(fieldContent.c_str()));
                    pNodeTd->appendChild(pT);
                }
                else
                {
                    // encode as base64
                    variant_t v;
                    v.vt = VT_ARRAY | VT_UI1;
                    SAFEARRAYBOUND  rgsabound[1];
                    rgsabound[0].cElements = fieldContent.length() * sizeof(_TCHAR);
                    rgsabound[0].lLbound = 0;
                    v.parray = SafeArrayCreate(VT_UI1, 1, rgsabound);
                    void* pArrayData = NULL;
                    SafeArrayAccessData(v.parray,&pArrayData);
                    memcpy(pArrayData, fieldContent.data(), fieldContent.length() * sizeof(_TCHAR));
                    SafeArrayUnaccessData(v.parray);
                    pElementTd->PutdataType(L"bin.base64");
                    pElementTd->nodeTypedValue = v;
                    SafeArrayDestroyData(v.parray);
                }
                break; }

            case 'v': { // binary data

                // MSDN: "Binary data is stored with an index name created by 
                //        concatenating the table name and the values of the 
                //        record's primary keys using a period delimiter."
                tstring index = table + tssIdx.str();

                if (m_mediaIds.find(index) != m_mediaIds.end())
                {
                    tstring href = _T("media:");
                    href += index;
                    pElementTd->setAttribute(L"href", href.c_str());
                }
                else if (m_streamIds.find(index) == m_streamIds.end()) 
                {
                    dumpBinaryStream(index.c_str(), pElementTd, hRecRow, col);
                }

                break; }

            default:
                break;
            }
        }

        indent(pNodeRow, 2);
    }
}

//------------------------------------------------------------------------------
//
// Dump _Streams table
//
//------------------------------------------------------------------------------
void Msi2Xml::dumpStreams(xml::IXMLDOMNode* parentNode)
{
    // emit "Name"
    indent(parentNode, 2);
    xml::IXMLDOMElementPtr pElementName(m_doc->createElement(L"col"));
    pElementName->appendChild(m_doc->createTextNode(L"Name"));
    pElementName->setAttribute(L"key", L"yes");
    pElementName->setAttribute(L"def", L"s62");
    parentNode->appendChild(pElementName);

    // emit "Data"
    indent(parentNode, 2);
    xml::IXMLDOMElementPtr pElementData(m_doc->createElement(L"col"));
    pElementData->appendChild(m_doc->createTextNode(L"Data"));
    pElementData->setAttribute(L"def", L"V0");
    parentNode->appendChild(pElementData);

    // list the rows
    SmrtMsiHandle hViewRows;
    OK(MsiDatabaseOpenView(m_db, _T("SELECT * FROM `_Streams`"), &hViewRows));
    OK(MsiViewExecute(hViewRows, NULL));

    // determine column type
    SmrtMsiHandle hRecInfo;
    OK(MsiViewGetColumnInfo(hViewRows, MSICOLINFO_TYPES, &hRecInfo));

    // prepare key container
    typedef std::deque<tstring> Idx;
    Idx idx;

    // iterate over rows
    SmrtMsiHandle hRecRow;
    UINT res;
    while ((res = MsiViewFetch(hViewRows, &hRecRow)) != ERROR_NO_MORE_ITEMS) 
    {
        tstring strId = recordGetString(hRecRow, 1);

        if (strId[0] == 0x05) 
            continue; // don't write internal streams

        if (m_streamIds.find(strId) != m_streamIds.end()
            && m_mediaIds.find(strId) == m_mediaIds.end()) 
        {
            continue; // already extracted
        }

        // search insert location
        Idx::iterator it = std::lower_bound(idx.begin(), idx.end(), strId);

        // retrieve node at insert location
        tostringstream oss;
        oss << _T("row[") << std::distance(idx.begin(), it) << _T("]");
        xml::IXMLDOMNodePtr pInTd(parentNode->selectSingleNode(oss.str().c_str()));

        // insert new index
        idx.insert(it, strId);

        // get pointer to row at insert location for new row
        xml::IXMLDOMNodePtr pInRow(pInTd ? pInTd->previousSibling : NULL);

        // create and add "row" element
        indent(parentNode, 2, pInRow);
        xml::IXMLDOMElementPtr pElementRow(m_doc->createElement(L"row"));
        xml::IXMLDOMNodePtr pNodeRow(parentNode->insertBefore(pElementRow, 
            _variant_t(pInRow, pInRow != NULL)));

        // write "Name" entry
        indent(pNodeRow, 3);
        xml::IXMLDOMElementPtr pElementTd1(m_doc->createElement(L"td"));
        xml::IXMLDOMNodePtr pNodeTd1(pNodeRow->appendChild(pElementTd1));
        pNodeTd1->appendChild(m_doc->createTextNode(strId.c_str()));

        // write "Data" entry
        indent(pNodeRow, 3);
        xml::IXMLDOMElementPtr pElementTd2(m_doc->createElement(L"td"));
        xml::IXMLDOMNodePtr pNodeTd2(pNodeRow->appendChild(pElementTd2));
        if (MsiRecordIsNull(hRecRow, 2)) 
        {
            indent(pNodeRow, 2);
            continue;
        }

        if (m_mediaIds.find(strId) != m_mediaIds.end())
        {
            tstring href = _T("media:");
            href += strId;
            pElementTd2->setAttribute(L"href", href.c_str());
        }
        else
        {
            dumpBinaryStream(strId.c_str(), pElementTd2, hRecRow, 2);
        }

        indent(pNodeRow, 2);
    }
}

//------------------------------------------------------------------------------
// Get msi codepage
//------------------------------------------------------------------------------
long Msi2Xml::codePage() const
{
    unsigned codePage;
    char tmpDir[MAX_PATH];
    char tmpFile[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpDir);
    GetTempFileNameA(tmpDir, "msi", 0, tmpFile);
    OK(MsiDatabaseExportA(m_db, "_ForceCodepage", tmpDir, tmpFile + strlen(tmpDir)));
    try 
    {
        std::ifstream is(tmpFile);
        is.ignore((std::numeric_limits<int>::max)(), is.widen('\n'));
        is.ignore((std::numeric_limits<int>::max)(), is.widen('\n'));
        std::string str;
        if (!(is >> codePage >> str) || str != "_ForceCodepage") 
        {
            tcerr << color::red << _T("Unable to determine codepage") << color::base << std::endl;
            _com_issue_error(E_FAIL);
        }
    }
    catch (...) 
    {
        DeleteFileA(tmpFile);
        throw;
    }
    DeleteFileA(tmpFile);
    return (long)codePage;
}

//------------------------------------------------------------------------------
// Extract binary stream
//------------------------------------------------------------------------------
void Msi2Xml::dumpBinaryStream(LPCTSTR id, 
                               xml::IXMLDOMElement* elm, 
                               MSIHANDLE row, UINT column)
{
    // save Id so it does not get extracted in the _Streams table
    m_streamIds.insert(id);

    tstring strBinFile(m_binDir + id);
    tstring strBinHref;  
    if (!m_binDirRel.empty()) 
    {
        strBinHref = m_binDirRel + tstring(_T("/")) + id;
    }
    else 
    {
        strBinHref = id;
    }

    if (!m_dumpStreams) 
    {
        // Encode binary data with base64
        // (MSXML can do this encoding for you, but for memory efficiency
        //  and better indenting I opted for encoding the data myself.)
        
        // set node type
        elm->PutdataType(L"bin.base64");

        // calculated character count of Base64 encoded binary data
        UINT dwSize = MsiRecordDataSize(row, column);
        UINT dwChars = 4 * dwSize / 3 + 4;
        dwChars += dwChars / 72 + 7;
        std::vector<WCHAR> bufEncoded(dwChars);
        bufEncoded[0] = L'\n';

        // init MD5 checksum
        MD5_CTX ctx;
        MD5Init(&ctx);

        DWORD cbBufIn = CHUNK_BIN;
        std::vector<char> bufBinary(cbBufIn);
        long lBufOutOff = 1;

        do 
        {
            // read next binary chunk
            OK(MsiRecordReadStream(row, column, &bufBinary[0], &cbBufIn));

            // update MD5 context
            MD5Update(&ctx, (LPCVOID)&bufBinary[0], cbBufIn, 1);

            // encode to base64
            assert(lBufOutOff + 4 * cbBufIn / 3 + 5 < dwChars);
            int cbBufOut = b64_ntop((LPBYTE)&bufBinary[0], cbBufIn, &bufEncoded[0] + lBufOutOff, CHUNK_BASE64);

            if (cbBufOut == -1)
                _com_issue_error(ERROR_INVALID_FUNCTION);

            // append newline
            bufEncoded[lBufOutOff + cbBufOut] = L'\n';
            lBufOutOff += cbBufOut + 1;
        } while (cbBufIn == CHUNK_BIN);

        bufEncoded[lBufOutOff++] = L'\t';
        bufEncoded[lBufOutOff++] = L'\t';
        bufEncoded[lBufOutOff++] = L'\t';
        bufEncoded[lBufOutOff++] = L'\0';

        // create and append text node
        BSTR bstr = SysAllocStringLen(&bufEncoded[0], lBufOutOff);

        // append as text node (_bstr_t takes ownership of 'bstr' !)
        elm->appendChild(m_doc->createTextNode(_bstr_t(bstr, false)));

        // finalize MD5 and generate "md5" attribute
        MD5Final(&ctx);
        tostringstream oss;
        oss.fill(_T('0'));
        oss.setf(std::ios::hex, std::ios::basefield);
        for (int j = 0; j < 16; ++j) 
        {
            oss << std::setw(2) << static_cast<unsigned int>(ctx.digest[j]);
        }
        elm->setAttribute(L"md5", oss.str().c_str());
    }
    else 
    {
        // read binary data
        DWORD cbSize = MsiRecordDataSize(row, column);
        std::vector<char> bufBinary(cbSize+1);
        OK(MsiRecordReadStream(row, column, &bufBinary[0], &cbSize));

        // calculate MD5 checksum
        MD5_CTX ctx;
        MD5Init(&ctx);
        MD5Update(&ctx, (LPCVOID)&bufBinary[0], cbSize, sizeof(CHAR));
        MD5Final(&ctx);

        // set href attribute
        elm->setAttribute(L"href", strBinHref.c_str());

        // write MD5 checksum
        tostringstream oss;
        oss.fill(_T('0'));
        oss.setf(std::ios::hex, std::ios::basefield);
        for (int j = 0; j < 16; ++j) 
        {
            oss << std::setw(2) << static_cast<unsigned int>(ctx.digest[j]);
        }
        elm->setAttribute(L"md5", oss.str().c_str());

        // check if file exists
        if (GetFileAttributes(strBinFile.c_str()) != 0xFFFFFFFF) 
        {
            // check existing file's MD5 checksum

            // map file to memory
            SmrtFileHandle hFile(
                CreateFile(strBinFile.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                            NULL, OPEN_EXISTING, NULL, NULL));

            if (hFile == INVALID_HANDLE_VALUE)
                _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

            if (GetFileSize(hFile, NULL) == cbSize && cbSize > 0) 
            {
                // create file mapping
                SmrtFileHandle hMap(CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL));
                if (hMap == NULL)
                    _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

                // map file
                SmrtFileMap pMap(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0));

                // compare with new data
                if (memcmp((LPCVOID)pMap, (LPCVOID)&bufBinary[0], cbSize) == 0)
                    return; // same, don't write new data
            }
        }

        SmrtFileHandle hFile(
            CreateFile(strBinFile.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL));

        if (hFile == INVALID_HANDLE_VALUE) 
        {
            tcerr << color::red << _T("Error creating binary file ") 
                << strBinFile << _T(": ") << color::base ;
            _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
        }

        DWORD dwWritten = 0;
        WriteFile(hFile, (LPCVOID)&bufBinary[0], cbSize, &dwWritten, NULL);
    }
}


//------------------------------------------------------------------------------
//
// Indent
//
//------------------------------------------------------------------------------
void Msi2Xml::indent(xml::IXMLDOMNode* parentNode, unsigned level, xml::IXMLDOMNode* before)
{
    WCHAR indent[] = L"\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
    indent[level + 1] = L'\0';
    parentNode->insertBefore(m_doc->createTextNode(indent), _variant_t(static_cast<IUnknown*>(before), before != NULL));
}

//------------------------------------------------------------------------------
// Print banner message
//------------------------------------------------------------------------------
void Msi2Xml::printBanner() const
{
    if (!m_nologo)
    {
        tcerr << _T("msi2xml ") << moduleVersion();
#ifdef _UNICODE
        tcerr << _T(" (Unicode)");
#endif
        tcerr << _T(", Copyright (C) 2001-2005 Daniel Gehriger") << std::endl << std::endl;
        tcerr << _T("msi2xml comes with ABSOLUTELY NO WARRANTY.") << std::endl;
        tcerr << _T("This is free software, and you are welcome to redistribute it") << std::endl;
        tcerr << _T("under certain conditions; use the '-l' option for details.") << std::endl << std::endl;
    }
}

//------------------------------------------------------------------------------
// Print usage message
//------------------------------------------------------------------------------
void Msi2Xml::printUsage() const
{
    tcerr << _T("\nUsage: ") << std::endl;
    tcerr << _T("msi2xml [-q] [-n] [-d] [-m] [-e ENCODING] [-s [STYLESHEET]] [-b [DIR]]") << std::endl;
    tcerr << _T("        [-c [DIR[,MEDIAS]]] [-o XMLFILE] file\n");
    tcerr << _T(" -Q --nologo                   don't print banner message") << std::endl;
    tcerr << _T(" -q --quiet                    quiet processing") << std::endl;
    tcerr << _T(" -n --no-sort                  disable sorting of rows") << std::endl;
    tcerr << _T(" -m --merge-module             convert a merge module (.msm)") << std::endl;
    tcerr << _T(" -e --encoding=ENCODING        force XML encoding to ENCODING (default is US-ASCII)") << std::endl;
    tcerr << _T(" -s --stylesheet               disable default XSL stylesheet") << std::endl;
    tcerr << _T(" -s --stylesheet=NAME          use XSL stylesheet NAME") << std::endl;
    tcerr << _T(" -b --dump-streams=DIR         save binary streams to DIR subdirectory") << std::endl;
    tcerr << _T(" -c --extract-cabs=DIR,MEDIAS  extract content of cabinets (for MEDIAS) to DIR") << std::endl;
    tcerr << _T(" -o --output=FILE              write MSI file to FILE") << std::endl;
    tcerr << std::endl;
}

//------------------------------------------------------------------------------
// Parse command line
//------------------------------------------------------------------------------
void Msi2Xml::parseCommandLine(int argc, _TCHAR* argv[])
{
    _TCHAR drive[_MAX_DRIVE];
    _TCHAR dir[_MAX_DIR];
    _TCHAR fname[_MAX_FNAME];
    _TCHAR ext[_MAX_EXT];

    // short option string (option letters followed by a colon ':' require an argument)
    static const _TCHAR optstring[] = _T("qQdmnls:b:o:e:c:");

    // mapping of long to short arguments
    static const Option longopts[] = 
    {
        // general options
        { _T("license"),            no_argument,        NULL,   _T('l') },
        { _T("nologo"),             no_argument,        NULL,   _T('Q') },
        { _T("quiet"),              no_argument,        NULL,   _T('q') },
        { _T("encoding"),           required_argument,  NULL,   _T('e') },
        { _T("merge-module"),       no_argument,        NULL,   _T('m') },
        { _T("no-sort"),            no_argument,        NULL,   _T('n') },
        { _T("stylesheet"),         optional_argument,  NULL,   _T('s') },
        { _T("dump-streams"),       optional_argument,  NULL,   _T('b') },
        { _T("extract-cabs"),       optional_argument,  NULL,   _T('c') },
        { _T("output"),             required_argument,  NULL,   _T('o') },
        { NULL,                     0,                  NULL,   0       }
    };

    int c;
    int longIdx = 0;
    while ((c = getopt_long(argc, argv, optstring, longopts, &longIdx)) != -1) 
    {
        if (optarg != NULL && (*optarg == _T('-') || *optarg == _T('/'))) 
        {
            optarg = NULL;
            --optind;
        }

        switch (c) 
        {
        case _T('q'):  // run quiet
            ++m_quiet;
            break;

        case _T('Q'):  // no banner
            m_nologo = true;
            break;

        case _T('n'):  // disable row sorting
            m_sortRows = false;
            break;

        case _T('m'):  // convert merge module
            m_mergeModule = true;
            break;

        case _T('s'):  // custom style sheet
            m_useDefaultStyleSheet = false;
            if (optarg) m_styleSheet = optarg;
            break;

        case _T('b'):  // custom bin dir
            m_dumpStreams = true;
            if (optarg) m_binDirRel = optarg;
            if (m_binDirRel.find_first_of(_T(":/\\")) != tstring::npos) 
            {
                printBanner();
                tcerr << color::red << _T("Invalid folder name specified: ") 
                    << m_binDirRel << color::base << std::endl << std::endl;
                printUsage();
                exit(2);
            }
            break;

        case _T('c'):  // extract cabinet files
            m_extractCabs = true;
            if (optarg) 
            {
                _TCHAR* nextToken;
                m_cabDirRel = _tcstok_s(optarg, _T(","), &nextToken);
                while (LPTSTR media = _tcstok_s(NULL, _T(","), &nextToken))
                {
                    m_mediasToExtract.insert(media);
                    m_mediasToExtract.insert(tstring(_T("#")) + media);
                }
            }
            if (m_cabDirRel.find_first_of(_T(":/\\")) != tstring::npos) 
            {
                printBanner();
                tcerr << color::red << _T("Invalid folder name specified: ") 
                    << m_cabDirRel << color::base << std::endl << std::endl;
                printUsage();
                exit(2);
            }
            break;

        case _T('o'):  // out file
            if (optarg)
            {
                m_outputPath = optarg;
            }
            break;


        case _T('l'):  // show license and exit
            printBanner();
            std::cerr << loadTextResource(IDR_GPL);
            exit(0);

        case _T('e'):  // force XML encoding
            if (!optarg) 
            {
                printBanner();
                tcerr << color::red << _T("Missing encoding value.") 
                    << color::base  << std::endl << std::endl;
                printUsage();
                exit(2);
            }
            m_encoding = optarg;
            break;

        case _T('?'):  // invalid argument
            printBanner();
            printUsage();
            exit(2);
        }
    }

    printBanner();
    if (optind != argc - 1) 
    {
        printUsage();
        exit(2);
    }

    if (argv[optind])
    {
        m_inputPath = argv[optind];
    }

    // get current directory
    _TCHAR buf[MAX_PATH+1];
    GetCurrentDirectory(MAX_PATH, buf);
    tstring currentDir = buf + tstring(_T("\\"));

    // prepend current directory if relative path
    if (!m_inputPath.empty()) 
    {
        _tsplitpath_s(m_inputPath.c_str(), 
                      drive, ARRAYSIZE(drive), 
                      dir, ARRAYSIZE(dir), 
                      fname, ARRAYSIZE(fname), 
                      ext, ARRAYSIZE(ext));
        if (drive[0] == '\0' && dir[0] != '\\') 
        {
            m_inputPath = currentDir + m_inputPath;
        }

        _tsplitpath_s(m_inputPath.c_str(), 
                      drive, ARRAYSIZE(drive), 
                      dir, ARRAYSIZE(dir), 
                      fname, ARRAYSIZE(fname), 
                      ext, ARRAYSIZE(ext));
        _tmakepath_s(buf, ARRAYSIZE(buf), drive, dir, NULL, NULL);
        m_inputDir = buf;
    }

    // determine output folder
    if (!m_outputPath.empty()) 
    {
        _tsplitpath_s(m_outputPath.c_str(), 
                      drive, ARRAYSIZE(drive), 
                      dir, ARRAYSIZE(dir), 
                      fname, ARRAYSIZE(fname), 
                      ext, ARRAYSIZE(ext));
        if (drive[0] == '\0' && dir[0] != '\\') 
        {
            m_outputPath = currentDir + m_outputPath;
        }
    }
    else
    {
        // create from input file
        _tsplitpath_s(m_inputPath.c_str(), 
                      drive, ARRAYSIZE(drive), 
                      dir, ARRAYSIZE(dir), 
                      fname, ARRAYSIZE(fname), 
                      ext, ARRAYSIZE(ext));
        _tmakepath_s(buf, ARRAYSIZE(buf), NULL, currentDir.c_str(), fname, _T("xml"));
        m_outputPath = buf;
    }

    if (!m_outputPath.empty())
    {
        _tsplitpath_s(m_outputPath.c_str(), 
                      drive, ARRAYSIZE(drive), 
                      dir, ARRAYSIZE(dir), 
                      fname, ARRAYSIZE(fname), 
                      ext, ARRAYSIZE(ext));
        _tmakepath_s(buf, ARRAYSIZE(buf), drive, dir, NULL, NULL);
        m_outputDir = buf;
    }

    // Write default style sheet
    if (m_useDefaultStyleSheet) 
    {
        _TCHAR szStyleSheet[MAX_PATH];
        _tsplitpath_s(m_outputPath.c_str(), drive, ARRAYSIZE(drive), dir, ARRAYSIZE(dir), NULL, 0, NULL, 0);
        _tmakepath_s(szStyleSheet, ARRAYSIZE(szStyleSheet), drive, dir, _T("msi.xsl"), NULL);
        SmrtFileHandle hFile(CreateFile(szStyleSheet,
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL));

        if (hFile == INVALID_HANDLE_VALUE) 
        {
            tcerr << color::red << szStyleSheet << _T(": ") << color::base;
            _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
        }

        DWORD dwWritten = 0;
        std::string xslText = loadTextResource(IDR_MSI_DT_XSL);
        WriteFile(hFile, (LPCVOID)xslText.c_str(), xslText.length(), &dwWritten, NULL);
    }

    // Create directory for binary files
    if (m_dumpStreams) 
    {
        m_binDir = m_outputDir + m_binDirRel + _T('\\');
        

        if (!m_binDirRel.empty()) 
        {
            DWORD dwAtt = GetFileAttributes(m_binDir.c_str());
            if (dwAtt == 0xFFFFFFFF || (dwAtt & FILE_ATTRIBUTE_DIRECTORY) == 0) 
            {
                if (CreateDirectory(m_binDir.c_str(), NULL) == 0) 
                {
                    tcerr << color::red << m_binDir << _T(": ") << color::base;
                    _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
                }
            }
        }
    }

    // Create directory for cab files
    if (m_extractCabs) 
    {
        m_cabDir = m_outputDir + m_cabDirRel + _T('\\');

        if (!m_cabDirRel.empty()) 
        {
            DWORD dwAtt = GetFileAttributes(m_cabDir.c_str());
            if (dwAtt == 0xFFFFFFFF || (dwAtt & FILE_ATTRIBUTE_DIRECTORY) == 0) 
            {
                if (CreateDirectory(m_cabDir.c_str(), NULL) == 0) 
                {
                    tcerr << color::red << m_cabDir << _T(": ") << color::base;
                    _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
                }
            }
        }
    }
}

//------------------------------------------------------------------------------
tstring Msi2Xml::recordGetString(MSIHANDLE record, UINT col)
{
    static tcharvector buf(BUF_SIZE);
    DWORD len;
    UINT res;

    while ((res = MsiRecordGetString(record, col, &buf[0], &(len = buf.size()))) == ERROR_MORE_DATA) 
    {
        buf.resize(len+1); 
    }
    OK(res);

    return &buf[0];
}

//------------------------------------------------------------------------------
tstring Msi2Xml::moduleVersion()
{
    _TCHAR file_name[MAX_PATH];
    GetModuleFileName(GetModuleHandle(NULL), file_name, MAX_PATH);

    DWORD dwDummyHandle; 
    DWORD len = GetFileVersionInfoSize( file_name, &dwDummyHandle );

    std::vector<_TCHAR> buf(len);
    GetFileVersionInfo( file_name, 0, len, &buf[0]);
    unsigned int ver_length;
    LPVOID lpvi;
    VerQueryValue(&buf[0], _T("\\"), &lpvi, &ver_length);

    VS_FIXEDFILEINFO fileInfo;
    fileInfo = *reinterpret_cast<VS_FIXEDFILEINFO*>(lpvi);
    tostringstream s;
    s << (unsigned)HIWORD(fileInfo.dwFileVersionMS) << _T(".")
      << (unsigned)LOWORD(fileInfo.dwFileVersionMS) << _T(".")
      << (unsigned)HIWORD(fileInfo.dwFileVersionLS) << _T(".")
      << (unsigned)LOWORD(fileInfo.dwFileVersionLS);
    return s.str();
}

//------------------------------------------------------------------------------
// Load text resource
//
// Parameters:
//
//  resourceId             - resource ID
//
// Returns:
//
//  pointer to text array
//------------------------------------------------------------------------------
std::string Msi2Xml::loadTextResource(WORD resourceId)
{
    HRSRC hXMLTempl = FindResource(NULL, MAKEINTRESOURCE(resourceId), _T("TEXT"));

    if (hXMLTempl == NULL)
        _com_issue_error(ERROR_INVALID_HANDLE);

    HGLOBAL hResXMLTempl = LoadResource(NULL, hXMLTempl);

    if (hResXMLTempl == NULL)
        _com_issue_error(ERROR_INVALID_HANDLE);

    LPCSTR szXMLTempl = (LPCSTR)LockResource(hResXMLTempl);

    if (!szXMLTempl )
        _com_issue_error(ERROR_INVALID_HANDLE);

    DWORD len = SizeofResource(NULL, hXMLTempl);

    return std::string(szXMLTempl, len);
}

//------------------------------------------------------------------------------
bool Msi2Xml::validCharacters(const tstring& str)
{
    for (tstring::const_iterator it = str.begin(); it != str.end(); ++it)
    {
        _TCHAR c = *it;
        if (c != 0x09 
            && c != 0x0a 
            && c != 0x0d 
            && !(c >= 0x20 && c <= 0xd7ff)
            && !(c >= 0xe000 && c <= 0xfffd))
        {
            return false;
        }
    }

    return true;
}
