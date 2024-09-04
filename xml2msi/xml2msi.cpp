//------------------------------------------------------------------------------
//
// $Id: xml2msi.cpp 105 2007-09-27 14:23:27Z dgehriger $
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
#include "xml2msi.h"
#include "coldefs.h"
#include "resource.h"
#include "md5.h"
#include "base64.h"
#include "getopt.h"
#include "consolecolor.h"
#include <atlcomcli.h>

#if (_WIN32_MSI <  150)
typedef struct _MSIFILEHASHINFO {
    ULONG dwFileHashInfoSize;
    ULONG dwData [ 4 ];
} MSIFILEHASHINFO, *PMSIFILEHASHINFO;

typedef UINT (WINAPI *PFN_MSIGETFILEHASH)(LPCTSTR, DWORD, PMSIFILEHASHINFO);
static PFN_MSIGETFILEHASH MsiGetFileHash = NULL;
#endif // (_WIN32_MSI <  150)


//------------------------------------------------------------------------------
// Main entry
//------------------------------------------------------------------------------
int __cdecl _tmain(int argc, _TCHAR* argv[], _TCHAR* envp[])
{
    int exitCode = 0;

    // Initialize COM
    CoInitialize(NULL);

    // dynamically load file hash function
#if (_WIN32_MSI <  150)
    SmrtLibHandle hLibMsi(LoadLibrary(_T("msi.dll")));
    if (!hLibMsi.isNull())
    {
#ifdef _UNICODE
        MsiGetFileHash = reinterpret_cast<PFN_MSIGETFILEHASH>(GetProcAddress(hLibMsi, "MsiGetFileHashW"));
#else
        MsiGetFileHash = reinterpret_cast<PFN_MSIGETFILEHASH>(GetProcAddress(hLibMsi, "MsiGetFileHashA"));
#endif

        if (MsiGetFileHash == NULL)
        {
            tcerr << color::yellow << _T("Warning: updating 'MsiFileHash' table requires at least Windows Installer 2.0") << color::base << std::endl;
        }
    }
#endif // (_WIN32_MSI <  150)

    {
        // create Xml2Msi instance
        Xml2Msi xml2msi(argc, argv);

        try 
        {
            // go
            xml2msi.create();
        }
        catch (const _com_error& e) 
        {
            if (e.Error() != E_FAIL) 
            {
                tcerr << color::red << _T("Error: ") << e.ErrorMessage() << std::endl;
            }

            if (!xml2msi.currentTable().empty()) 
            {
                tcerr << color::red << _T("Location: table \"") << xml2msi.currentTable() << _T("\"");
                if (xml2msi.currentRow() > 0) tcerr << _T(", row ") << xml2msi.currentRow();
                if (xml2msi.currentColumn() > 0) tcerr << _T(", column ") << xml2msi.currentColumn();
                tcerr << std::endl;
            }

            tcerr << color::base;
            exitCode = 1;
        }
        catch (const std::runtime_error& e)
        {
            if (e.what() == "")
            {
                tcerr << color::red << _T("Error: Failed to convert the MSI database!") 
                    << color::base << std::endl;
            }
            else
            {
                std::cerr << color::red << _T("Error: ") << e.what() 
                    << color::base << std::endl;
            }
        }
    }

    CoUninitialize( );
    return exitCode;
}

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
Xml2Msi::Xml2Msi(int argc, _TCHAR* argv[]) :
    m_currentCol(0),
    m_currentRow(0),
    m_compression(CabCompress::compMSZIP),
    m_quiet(0),
    m_nologo(false),
    m_checkMD5(true),
    m_udpateXml(false),
    m_updateUpgradeVersion(false),
    m_mergeModule(false),
    m_fixExtension(false),
	m_componentCode(false)
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

    // prepare a temporary folder for internal CABs
    _TCHAR tmpDir[MAX_PATH];
    _TCHAR tmpDirDir[MAX_PATH];
    GetTempPath(MAX_PATH, tmpDir);
    GetTempFileName(tmpDir, _T("cab"), 0, tmpDirDir);
    DeleteFile(tmpDirDir);
    CreateDirectory(tmpDirDir, NULL);
    _tcscat_s(tmpDirDir, ARRAYSIZE(tmpDirDir), _T("\\"));
    m_tempCabDir = tmpDirDir;
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
Xml2Msi::~Xml2Msi()
{
    // delete temporary file
    if (!m_tempPath.empty()) 
    {
        DeleteFile(m_tempPath.c_str());
    }

    // delete temporary cabs
    tstring findMask = m_tempCabDir + _T("*");
    WIN32_FIND_DATA ffd;
    SmrtFindHandle hFind(FindFirstFile(findMask.c_str(), &ffd));
    while (!hFind.isNull())
    {
        tstring path = m_tempCabDir + ffd.cFileName;
        DeleteFile(path.c_str());

        if (!FindNextFile(hFind, &ffd))
            break;
    }
    FindClose(hFind.release());

    // remove temporary cabinet folder
    RemoveDirectory(m_tempCabDir.c_str());
}

//------------------------------------------------------------------------------
// Create MSI
//------------------------------------------------------------------------------
void Xml2Msi::create()
{
    // Load the XML file
    m_doc->async = VARIANT_FALSE;
    m_doc->validateOnParse = VARIANT_TRUE;
    m_doc->preserveWhiteSpace = VARIANT_TRUE;
    m_doc->setProperty(L"ProhibitDTD", VARIANT_FALSE);

    if (m_doc->load(m_inputPath.c_str()) == VARIANT_FALSE) 
    {
        xml::IXMLDOMParseErrorPtr pErr = m_doc->parseError;

        tcerr << color::red;
        if (pErr->errorCode == 0x800C0006)
        {
            tcerr << _T("File not found: ") << m_inputPath;
        }
        else
        {
            tcerr << _T("Error while parsing ") << m_inputPath << _T(":") << std::endl;
            if (pErr->line != 0) 
            {
                tcerr << _T("Line ") << pErr->line << _T("; Column ") << pErr->linepos << std::endl;
            }
            tcerr << (LPCTSTR)pErr->reason;
        }

        tcerr << color::base << std::endl;
        _com_issue_error(E_FAIL);
    }

    // check version
    if (wcscmp(m_doc->selectSingleNode(L"/msi/@version")->text, L"1.1") != 0 
        && wcscmp(m_doc->selectSingleNode(L"/msi/@version")->text, L"2.0") != 0)
    {
        tcerr << color::red << _T("This version of xml2msi only supports XML files") << std::endl;
        tcerr << _T("created by msi2xml version 1.x and 2.0") << color::base << std::endl;
        _com_issue_error(E_FAIL);
    }

    // get root node
    xml::IXMLDOMNodePtr msiNode(m_doc->selectSingleNode(L"/msi"));

    // is this a merge module ?
    if (xml::IXMLDOMNodePtr node = msiNode->attributes->getNamedItem(L"msm"))
    {
        _bstr_t isMsm = node->nodeValue;
        if (wcscmp(isMsm, L"yes") == 0)
        {
            m_mergeModule = true;

            if (m_fixExtension)
            {
                // fix file extension...
                _TCHAR buf[_MAX_PATH];
                _TCHAR drive[_MAX_DRIVE];
                _TCHAR dir[_MAX_DIR];
                _TCHAR fname[_MAX_FNAME];
                _TCHAR ext[_MAX_EXT];
                _tsplitpath_s(m_outputPath.c_str(), 
                              drive, ARRAYSIZE(drive), 
                              dir, ARRAYSIZE(dir), 
                              fname, ARRAYSIZE(fname), 
                              ext, ARRAYSIZE(ext));
                _tmakepath_s(buf, ARRAYSIZE(buf), drive, dir, fname, _T("MSM"));
                m_outputPath = buf;
            }
        }
    }

    // create database
    DeleteFile(m_outputPath.c_str());
    OK(MsiOpenDatabase(m_outputPath.c_str(), MSIDBOPEN_CREATE, &m_db));

    // set database codepage
    if (xml::IXMLDOMNodePtr node = msiNode->attributes->getNamedItem(L"codepage")) 
    {
        char szTmpDir[_MAX_PATH];
        char szTmpFile[_MAX_PATH];
        GetTempPathA(_MAX_PATH, szTmpDir);
        GetTempFileNameA(szTmpDir, "msi", 0, szTmpFile);
        std::ofstream os(szTmpFile);
        os << std::endl << std::endl << (LPCSTR)node->text << "\t_ForceCodepage" << std::endl;
        os.close();
        UINT res = MsiDatabaseImportA(m_db, szTmpDir, szTmpFile + strlen(szTmpDir));
        DeleteFileA(szTmpFile);
        if (res == ERROR_FUNCTION_FAILED) 
        {
            tcerr << color::red 
                  << _T("Unable to set database codepage to \"") 
                  << (LPCTSTR)node->text
                  << _T("\"") << color::base << std::endl;
        }
    }

    // remember compression method
    if (xml::IXMLDOMNodePtr node = msiNode->attributes->getNamedItem(L"compression"))
    {
        _bstr_t compr = node->nodeValue;
        if      (wcscmp(compr, L"MSZIP") == 0)      m_compression = CabCompress::compMSZIP;
        else if (wcscmp(compr, L"LZX") == 0)        m_compression = CabCompress::compLZX;
        else if (wcscmp(compr, L"Quantum") == 0)    m_compression = CabCompress::compQuantum;
        else if (wcscmp(compr, L"none") == 0)       m_compression = CabCompress::compNone;
    }


    // update XML from command line
    update();

    // generate CAB files
    buildCabinets();

    // create and populate the tables
    xml::IXMLDOMNodeListPtr pTables(m_doc->selectNodes(L"/msi/table"));
    xml::IXMLDOMNodePtr table;
    while ((table = pTables->nextNode()) != NULL) 
    {
        validateTable(table); // validate the table
        createTable(table); // create the table
        populateTable(table); // populate table
    }

    // create the summary information stream
    createSummaryInfo();
    OK(MsiDatabaseCommit(m_db));

    // write modified XML
    if (m_udpateXml)
    {
        OK(m_doc->save(m_xmlOutputPath.c_str()));
    }

}

//------------------------------------------------------------------------------
void Xml2Msi::update()
{
    // update product version
    if (!m_productVersion.empty())
    {
        xml::IXMLDOMNodePtr     productVersionRow(m_doc->selectSingleNode(L"/msi/table[@name='Property']/row[td[1]='ProductVersion']"));
        xml::IXMLDOMNodePtr     oldnode(productVersionRow->selectSingleNode(L"td[2]"));
        xml::IXMLDOMElementPtr  newnode(m_doc->createElement(L"td"));
        newnode->appendChild(m_doc->createTextNode(m_productVersion.c_str()));
        productVersionRow->replaceChild(newnode, oldnode);

        if (!m_quiet)
        {
            tcerr << color::green << _T("Updated product version to ") 
                  << m_productVersion << color::base << std::endl;
        }
    }

    // update "Upgrade" table
    if (m_updateUpgradeVersion)
    {
        // get upgrade code
        _bstr_t upgradeCode = m_doc->selectSingleNode(L"/msi/table[@name='Property']/row[td[1]='UpgradeCode']/td[2]")->nodeTypedValue;

        // get version
        if (m_upgradeVersion.empty())
        {
            m_upgradeVersion = (_bstr_t)m_doc->selectSingleNode(L"/msi/table[@name='Property']/row[td[1]='ProductVersion']/td[2]")->nodeTypedValue;
        }

        // locate matching entry in 'Upgrade' table
        tostringstream oss;
        oss << _T("/msi/table[@name='Upgrade']/row[td[1]='") << (LPCTSTR)upgradeCode << _T("']");

        xml::IXMLDOMNodeListPtr upgradeRows(m_doc->selectNodes(oss.str().c_str()));
        while (xml::IXMLDOMNodePtr row = upgradeRows->nextNode())
        {
            xml::IXMLDOMNodePtr     oldnode(row->selectSingleNode(L"td[3]"));
            xml::IXMLDOMElementPtr  newnode(m_doc->createElement(L"td"));
            newnode->appendChild(m_doc->createTextNode(m_upgradeVersion.c_str()));
            row->replaceChild(newnode, oldnode);

            // check that version number is excluding
            int attributes = nodeValue(row->selectSingleNode(L"td[5]"));
            if (attributes & msidbUpgradeAttributesVersionMaxInclusive)
            {
                attributes &= ~msidbUpgradeAttributesVersionMaxInclusive;

                tostringstream oss;
                oss << attributes;
                xml::IXMLDOMNodePtr     oldnode(row->selectSingleNode(L"td[5]"));
                xml::IXMLDOMElementPtr  newnode(m_doc->createElement(L"td"));
                newnode->appendChild(m_doc->createTextNode(oss.str().c_str()));
                row->replaceChild(newnode, oldnode);
            }

            if (!m_quiet)
            {
                tcerr << color::green << _T("Updated 'VersionMax' in 'Upgrade' table to ") 
                    << m_upgradeVersion << _T(" (excluding)") << color::base << std::endl;
            }
        }

    }

    // update package code
    if (!m_packageCode.empty())
    {
        xml::IXMLDOMNodePtr     summaryNode(m_doc->selectSingleNode(L"/msi/summary"));
        xml::IXMLDOMNodePtr     oldnode(summaryNode->selectSingleNode(L"revnumber"));
        xml::IXMLDOMElementPtr  newnode(m_doc->createElement(L"revnumber"));
        newnode->appendChild(m_doc->createTextNode(m_packageCode.c_str()));
        summaryNode->replaceChild(newnode, oldnode);

        if (!m_quiet)
        {
            tcerr << color::green << _T("Updated package code to ") 
                << m_packageCode << color::base << std::endl;
        }
    }

    // update product code
    if (!m_productCode.empty())
    {
        xml::IXMLDOMNodePtr     productCodeRow(m_doc->selectSingleNode(L"/msi/table[@name='Property']/row[td[1]='ProductCode']"));
        xml::IXMLDOMNodePtr     oldnode(productCodeRow->selectSingleNode(L"td[2]"));
        xml::IXMLDOMElementPtr  newnode(m_doc->createElement(L"td"));
        newnode->appendChild(m_doc->createTextNode(m_productCode.c_str()));
        productCodeRow->replaceChild(newnode, oldnode);

        if (!m_quiet)
        {
            tcerr << color::green << _T("Updated product code to ") 
                << m_productCode << color::base << std::endl;
        }
    }

    // update component code
    if (m_componentCode)
    {
        xml::IXMLDOMNodeListPtr componentCodeRows(m_doc->selectNodes(L"/msi/table[@name='Component']/row[td[2]]"));
        while (xml::IXMLDOMNodePtr row = componentCodeRows->nextNode())
        {
            xml::IXMLDOMNodePtr     oldnode(row->selectSingleNode(L"td[2]"));
            xml::IXMLDOMElementPtr  newnode(m_doc->createElement(L"td"));
			tstring m_guid = guid();
            newnode->appendChild(m_doc->createTextNode(m_guid.c_str()));
            row->replaceChild(newnode, oldnode);

            if (!m_quiet)
            {
                tcerr << color::green << _T("Updated component code to ") 
                    << m_guid.c_str() << color::base << std::endl;
            }
        }
    }

    // update upgrade code
    if (!m_upgradeCode.empty())
    {
        xml::IXMLDOMNodePtr     upgradeCodeRow(m_doc->selectSingleNode(L"/msi/table[@name='Property']/row[td[1]='UpgradeCode']"));
        xml::IXMLDOMNodePtr     oldnode(upgradeCodeRow->selectSingleNode(L"td[2]"));
        xml::IXMLDOMElementPtr  newnode(m_doc->createElement(L"td"));
        newnode->appendChild(m_doc->createTextNode(m_upgradeCode.c_str()));
        upgradeCodeRow->replaceChild(newnode, oldnode);

        if (!m_quiet)
        {
            tcerr << color::green << _T("Updated upgrade code to ") 
                << m_upgradeCode << color::base << std::endl;
        }
    }

    // set properties
    if (!m_propertyMap.empty())
    {
        for (PropertyMap::const_iterator it = m_propertyMap.begin(); it != m_propertyMap.end(); ++it)
        {
            tstring property = it->first;
            tstring value = it->second;

            std::wstring xpath = L"/msi/table[@name='Property']/row[td[1]='" + property + L"']";
            xml::IXMLDOMNodePtr propertyNode(m_doc->selectSingleNode(xpath.c_str()));

            if (propertyNode)
            {
                xml::IXMLDOMNodePtr     oldnode(propertyNode->selectSingleNode(L"td[2]"));
                xml::IXMLDOMElementPtr  newnode(m_doc->createElement(L"td"));
                newnode->appendChild(m_doc->createTextNode(value.c_str()));
                propertyNode->replaceChild(newnode, oldnode);

                if (!m_quiet)
                {
                    tcerr << color::green << _T("Updated property ") << property << _T(" = \"") 
                        << value << _T("\"") << color::base << std::endl;
                }
            }
            else
            {
                xml::IXMLDOMNodePtr propertyTable(m_doc->selectSingleNode(L"/msi/table[@name='Property']"));
                xml::IXMLDOMElementPtr  rowNode(m_doc->createElement(L"row"));
                xml::IXMLDOMElementPtr  propertyNode(m_doc->createElement(L"td"));
                propertyNode->appendChild(m_doc->createTextNode(property.c_str()));
                xml::IXMLDOMElementPtr  valueNode(m_doc->createElement(L"td"));
                valueNode->appendChild(m_doc->createTextNode(value.c_str()));

                rowNode->appendChild(propertyNode);
                rowNode->appendChild(valueNode);
                propertyTable->appendChild(rowNode);

                if (!m_quiet)
                {
                    tcerr << color::green << _T("Set property ") << property 
                        << _T(" = \"") << value << _T("\"") << color::base << std::endl;
                }
            }
        }
    }
}

//------------------------------------------------------------------------------
struct AscendingDiskId
{
    bool operator()(const ATL::CAdapt<xml::IXMLDOMNodePtr>& lhs, 
                    const ATL::CAdapt<xml::IXMLDOMNodePtr>& rhs) const
    {
        xml::IXMLDOMNodePtr diskIdLhs = lhs.m_T->selectSingleNode(L"td[1]");
        xml::IXMLDOMNodePtr diskIdRhs = rhs.m_T->selectSingleNode(L"td[1]");
        return diskIdLhs != NULL && diskIdRhs != NULL && (int)diskIdLhs->nodeTypedValue <= (int)diskIdRhs->nodeTypedValue;
    }
};

//------------------------------------------------------------------------------
// Create cab file
//------------------------------------------------------------------------------
void Xml2Msi::buildCabinets()
{
    m_currentTable = _T("File");

    // initial sequence numbers
    int mediaLastSequencePrev = 0;

    if (!m_mergeModule)
    {
        // get all rows from media table
        xml::IXMLDOMNodeListPtr mediaNodeList(m_doc->selectNodes(L"/msi/table[@name='Media']/row"));

        // sort according to DiskId
        typedef std::vector<ATL::CAdapt<xml::IXMLDOMNodePtr> > NodeList;
        NodeList nodeList;
        nodeList.reserve(mediaNodeList->length);
        for (xml::IXMLDOMNodePtr mediaNode = mediaNodeList->nextNode(); mediaNode != NULL; mediaNode = mediaNodeList->nextNode()) 
        {
            nodeList.push_back(mediaNode);
        }
        std::sort(nodeList.begin(), nodeList.end(), AscendingDiskId());

        // iterate over medias
        for (NodeList::iterator it = nodeList.begin(); it != nodeList.end(); ++it) 
        {
            xml::IXMLDOMNode* mediaNode = it->m_T;
            bool internal = false;

            // obtain cabinet name
            xml::IXMLDOMNodePtr mediaCabinetNode = mediaNode->selectSingleNode(L"td[4]");
            tstring cabinetName = (_bstr_t)mediaCabinetNode->nodeTypedValue;
            if (cabinetName.empty()) continue;
            if (cabinetName[0] == _T('#'))
            {
                cabinetName = cabinetName.substr(1);
                internal = true;
            }

            // determine LastSequence number
            xml::IXMLDOMNodePtr mediaLastSequenceNode = mediaNode->selectSingleNode(L"td[2]");
            int mediaLastSequence = mediaLastSequenceNode->nodeTypedValue;

            // compress
            if (compressFiles(cabinetName.c_str(), mediaLastSequencePrev+1, mediaLastSequence))
            {
                // copy all external cabinets to the output directory
                if (!internal)
                {
                    tstring filePath = m_tempCabDir + cabinetName;
                    CopyFile(filePath.c_str(), m_outputDir.c_str(), FALSE);
                }
            }

            mediaLastSequencePrev = mediaLastSequence;
        }
    }
    else
    {
        // determine range of sequence numbers
        int firstSequence = -1, lastSequence = -1;
        xml::IXMLDOMNodeListPtr fileNodeList(m_doc->selectNodes(L"/msi/table[@name='File']/row"));

        for (xml::IXMLDOMNodePtr fileNode = fileNodeList->nextNode(); fileNode != NULL; fileNode = fileNodeList->nextNode()) 
        {
            xml::IXMLDOMNodePtr sequenceNode(fileNode->selectSingleNode(L"td[8]"));
            int sequence = sequenceNode->nodeTypedValue;

            if (firstSequence == -1 || sequence < firstSequence)
            {
                firstSequence = sequence;
            }

            if (lastSequence == -1 || sequence > lastSequence)
            {
                lastSequence = sequence;
            }
        }

        // standard name
        tstring cabinetName = L"MergeModule.CABinet";
        compressFiles(cabinetName.c_str(), firstSequence, lastSequence);
    }
}


//------------------------------------------------------------------------------
bool Xml2Msi::compressFiles(LPCTSTR cabinetName, int firstSequence, int lastSequence)
{
    bool failed = false;

    // create cab context
    std::auto_ptr<CabCompress> cab(
        new CabCompress(m_tempCabDir.c_str(), cabinetName, 0, 0, 1));


    // "Each source disk contains all the files whose sequence numbers (as 
    // shown in the Sequence column of the File table) are less than or 
    // equal to the value in the LastSequence column, and greater than the 
    // LastSequence value of the previous disk (or greater than 0, for the 
    // first entry in the Media table)."
    for (int sequence = firstSequence; sequence <= lastSequence; ++sequence)
    {
        // select the file with corresponding Sequence number
        tostringstream oss;
        oss << _T("/msi/table[@name='File']/row[td[8] = ") << sequence << _T("]");
        xml::IXMLDOMNodePtr fileNode(m_doc->selectSingleNode(oss.str().c_str()));
        if (fileNode == NULL)
            continue;

        // get file name node
        xml::IXMLDOMNodePtr fileNameNode(fileNode->selectSingleNode(L"td[1]"));

        // determine file name
        _bstr_t fileName =  fileNameNode->nodeTypedValue;

        // determine file href
        xml::IXMLDOMNodePtr nodeHref(fileNameNode->attributes->getNamedItem(L"href"));
        if (nodeHref == NULL) 
        {
            // finalize cabinet
            delete cab.release();

            // and delete it right away
            tstring filePath = m_tempCabDir + cabinetName;
            DeleteFile(filePath.c_str());

            failed = true;
            break;
        }
        _bstr_t href = nodeHref->nodeValue;

        // compress file
        tstring filePath = resolveHref(fileNode->selectSingleNode(L"td[1]"));
        if (!m_quiet)
        {
            tcerr << _T("Compressing file '") << (LPCTSTR)fileName << _T("'") << std::endl;
        }

        // validate MD5
        if (fileNameNode->attributes->getNamedItem(L"md5") != NULL) 
        {
            // map file to memory
            SmrtFileHandle hFile(
                CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL));
            if (hFile == INVALID_HANDLE_VALUE) _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

            SmrtFileMap pMap;

            if (GetFileSize(hFile, NULL) > 0)
            {
                // create file mapping
                SmrtFileHandle hMap(CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL));
                if (hMap == NULL) _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

                // map file
                pMap = SmrtFileMap(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0));
                if (pMap.isNull()) _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
            }

            // check MD5
            checkMD5(fileNameNode, (LPCVOID)pMap, GetFileSize(hFile, NULL), sizeof(BYTE));

        }

        // compress file
        cab->addFile(_bstr_t(filePath.c_str()), fileName, m_compression);

        // update file size
        {
            SmrtFileHandle hFile(CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            LARGE_INTEGER size;
            if (!GetFileSizeEx(hFile, &size))
            {
                tcerr << color::red << _T("Error: unable to determine file size of '") 
                    << (LPCTSTR)fileName << _T("'\n") << color::base << std::endl;
                continue;
            }

            if (size.u.HighPart != 0)
            {
                tcerr << color::red << _T("Error: file size of '") << (LPCTSTR)fileName 
                    << _T("' too large.\n") << color::base << std::endl;
                continue;
            }

            tostringstream oss;
            oss << size.u.LowPart;
            xml::IXMLDOMNodePtr oldnode = fileNode->selectSingleNode(L"td[4]");
            if (oldnode->text != _bstr_t(oss.str().c_str()))
            {
                xml::IXMLDOMElementPtr etd(m_doc->createElement(L"td"));
                etd->appendChild(m_doc->createTextNode(oss.str().c_str()));
                fileNode->replaceChild(etd, oldnode);

                if (!m_quiet)
                {
                    tcerr << color::green << _T("    updated file size in 'File' table ('") 
                        << (LPCTSTR)oldnode->text << _T("'' => '") 
                        << oss.str() << _T("')") << color::base << std::endl;
                }
            }
        }


        // set version
        {
            xml::IXMLDOMNodePtr oldnode = fileNode->selectSingleNode(L"td[5]");

            // check if this is a companion file
            std::wstring xpath = L"/msi/table[@name='File']/row[td[1]='" + (_bstr_t)oldnode->nodeTypedValue + L"']";
            if (m_doc->selectNodes(xpath.c_str())->length == 0)
            {
                DWORD dw; 
                DWORD len = GetFileVersionInfoSize(const_cast<LPTSTR>(filePath.c_str()), &dw);
                tostringstream oss;
                if (len > 0)
                {
                    std::vector<_TCHAR> buf(len);
                    GetFileVersionInfo(const_cast<LPTSTR>(filePath.c_str()), 0, len, &buf[0]);
                    UINT vlen;
                    LPVOID lpvi;
                    VerQueryValue(&buf[0], _T("\\"), &lpvi, &vlen);

                    VS_FIXEDFILEINFO fileInfo;
                    fileInfo = *reinterpret_cast<VS_FIXEDFILEINFO*>(lpvi);
                    oss << static_cast<unsigned>(HIWORD(fileInfo.dwFileVersionMS)) << _T(".")
                        << static_cast<unsigned>(LOWORD(fileInfo.dwFileVersionMS)) << _T(".")
                        << static_cast<unsigned>(HIWORD(fileInfo.dwFileVersionLS)) << _T(".")
                        << static_cast<unsigned>(LOWORD(fileInfo.dwFileVersionLS));
                }

                if ((_bstr_t)oldnode->nodeTypedValue != _bstr_t(oss.str().c_str()))
                {
                    xml::IXMLDOMElementPtr etd(m_doc->createElement(L"td"));
                    etd->appendChild(m_doc->createTextNode(oss.str().c_str()));
                    fileNode->replaceChild(etd, oldnode);

                    if (!m_quiet)
                    {
                        tcerr << color::green << _T("    updated version info in 'File' table ('") 
                            << (LPCTSTR)(_bstr_t)oldnode->nodeTypedValue << _T("' => '") 
                            << oss.str() << _T("')") << color::base << std::endl;
                    }
                }
            }
        }

        // updating compression flag (msidbFileAttributesCompressed)
        {
            xml::IXMLDOMNodePtr oldnode = fileNode->selectSingleNode(L"td[7]");
            UINT flags = nodeValue(oldnode);
            UINT wordcount = (UINT)m_doc->selectSingleNode(L"/msi/summary/wordcount")->nodeTypedValue;

            if (flags & msidbFileAttributesNoncompressed || (wordcount & msidbSumInfoSourceTypeCompressed) == 0)
            {
                // turn off explicit non-compressed flag
                flags &= ~msidbFileAttributesNoncompressed;

                // force compression flag if word count specifies uncompressed
                if ((wordcount & msidbSumInfoSourceTypeCompressed) == 0)
                {
                    flags |= msidbFileAttributesCompressed;
                }

                tostringstream oss;
                oss << flags;
                xml::IXMLDOMElementPtr etd(m_doc->createElement(L"td"));
                etd->appendChild(m_doc->createTextNode(oss.str().c_str()));
                fileNode->replaceChild(etd, oldnode);

                if (!m_quiet)
                {
                    tcerr << color::green << _T("    updated compression flags in 'File' table")
                        << color::base << std::endl;
                }

            }
        }

        // updating MsiFileHash table
        {
            // check if a FileHash table entry exists
            std::wstring xpath = L"/msi/table[@name='MsiFileHash']/row[td[1]='" + fileName + L"']";
            xml::IXMLDOMNodePtr fileHashNode(m_doc->selectSingleNode(xpath.c_str()));
            if ((fileHashNode != NULL) && (MsiGetFileHash != NULL))
            {
                MSIFILEHASHINFO fhi = { sizeof MSIFILEHASHINFO };
                OK(MsiGetFileHash(filePath.c_str(), 0, &fhi));

                xml::IXMLDOMNodePtr oldHash[4];
                oldHash[0] = fileHashNode->selectSingleNode(L"td[3]");
                oldHash[1] = fileHashNode->selectSingleNode(L"td[4]");
                oldHash[2] = fileHashNode->selectSingleNode(L"td[5]");
                oldHash[3] = fileHashNode->selectSingleNode(L"td[6]");

                bool updated = false;
                for (int i = 0; i < 4; ++i)
                {
                    if (static_cast<LONG>(oldHash[i]->nodeTypedValue) != static_cast<LONG>(fhi.dwData[i]))
                    {
                        tostringstream oss;
                        oss << static_cast<LONG>(fhi.dwData[i]);

                        xml::IXMLDOMElementPtr etd(m_doc->createElement(L"td"));
                        etd->appendChild(m_doc->createTextNode(oss.str().c_str()));
                        fileHashNode->replaceChild(etd, oldHash[i]);

                        updated = true;
                    }
                }

                if (updated && !m_quiet)
                {
                    tcerr << color::green << _T("    updated hash in 'MsiFileHash' table") 
                        << color::base << std::endl;
                }
            }
        }

    }

    // finalize cabinet
    delete cab.release();

    return !failed;
}


//------------------------------------------------------------------------------
// Validate table
//------------------------------------------------------------------------------
void Xml2Msi::validateTable(xml::IXMLDOMNode* table)
{
    _bstr_t bstrTable = table->attributes->getNamedItem(L"name")->nodeValue;
    m_currentTable = (LPCTSTR)bstrTable;

    // look up table in default table list
    int tableCount;
    for (tableCount = ARRAYSIZE(colDefs) - 1; tableCount >= 0 ; --tableCount) 
    {
        // compare table
        if (m_currentTable == colDefs[tableCount].szTable)
            break;
    }
    if (tableCount < 0) 
    {
#ifdef _DEBUG
        if (!m_quiet) 
        {
            tcerr << color::yellow << _T("Skipping validation of unknown table \"") 
                << m_currentTable << _T("\"") << color::base << std::endl;
        }
#endif // _DEBUG
        return; // not found
    }

    // iterate over column definitions
    m_currentCol = 0;
    xml::IXMLDOMNodeListPtr pCols(table->selectNodes(L"col"));
    xml::IXMLDOMNodePtr pCol;
    while ((pCol = pCols->nextNode()) != NULL) 
    {
        ++m_currentCol;

        tstring strDefs(colDefs[tableCount].szDef);
        tstring::size_type nDef = 0;
        for (int i = 1; i < m_currentCol; ++i) 
        {
            nDef = strDefs.find(_T(';'), nDef);
            if (nDef == tstring::npos) return;
        }
        tstring strDef(strDefs.substr(nDef, 3));

        // check if column definition missing
        xml::IXMLDOMElementPtr pEl(pCol);
        if (pCol->attributes->getNamedItem(L"def") == NULL) 
        {
            // missing, use default
            pEl->setAttribute(_T("def"), &strDef[1]);
            pEl->setAttribute(_T("key"), strDef[0] == _T('Y') ? L"yes" : L"no");
        }
        else 
        {
            // present, check against default
            _bstr_t bstrColDef(pCol->attributes->getNamedItem(L"def")->nodeValue);
            _TCHAR t = ((LPCTSTR)bstrColDef)[0];

            switch (tolower(strDef[1])) 
            {
            case 's':
            case 'l':
                if (tolower(t) != 's' && tolower(t) != 'l') 
                {
                    tcerr << color::red << _T("Column must be defined as string") << color::base << std::endl;
                    _com_issue_error(E_FAIL);
                }
                break;

            case 'i':
                if (tolower(t) != 'i') 
                {
                    tcerr << color::red << _T("Column must be defined as integer") << color::base << std::endl;
                    _com_issue_error(E_FAIL);
                }
                break;

            case 'v':
                if (tolower(t) != 'v') 
                {
                    tcerr << color::red << _T("Column must be defined as binary stream") << color::base << std::endl;
                    _com_issue_error(E_FAIL);
                }
                break;

            default:
                break;
            }
        }
    }

    m_currentTable.erase();
    m_currentCol = 0;
}

//------------------------------------------------------------------------------
// Create table
//------------------------------------------------------------------------------
void Xml2Msi::createTable(xml::IXMLDOMNode* table)
{
    _bstr_t bstrTable = table->attributes->getNamedItem(L"name")->nodeValue;
    if (!m_quiet) 
    {
        tcerr << _T("Populating table '") << (LPTSTR)bstrTable << _T("'") << std::endl;
    }

    m_currentTable = (LPCTSTR)bstrTable;

    if (bstrTable == _bstr_t(L"_Streams"))
        return;

    tostringstream ossSQL;
    ossSQL << _T("CREATE TABLE `") << (LPCTSTR)bstrTable << _T("` (");

    // add key columns to SQL query string
    xml::IXMLDOMNodeListPtr pColsKey(table->selectNodes(L"col[@key='yes' or @key='']"));
    { 
        if (pColsKey->length == 0) 
        {
            tcerr << color::red << _T("Missing primary key") << color::base << std::endl;
            _com_issue_error(E_FAIL);
        }
        xml::IXMLDOMNodePtr pCol = pColsKey->nextNode();
        while (pCol != NULL) 
        {
            ossSQL << buildSQLColSpec(pCol);
            pCol = pColsKey->nextNode();
            if (pCol) ossSQL << _T(", ");
        }
    }

    // add remaining columns to SQL query string
    { 
        xml::IXMLDOMNodeListPtr pCols(table->selectNodes(L"col[not(@key) or @key='no']"));
        xml::IXMLDOMNodePtr pCol = pCols->nextNode();
        while (pCol != NULL) 
        {
            if (pCol) ossSQL << _T(", ");
            ossSQL << buildSQLColSpec(pCol);
            pCol = pCols->nextNode();
        }
    }

    // add primary keys
    { 
        ossSQL << _T(" PRIMARY KEY ");
        pColsKey->reset();
        xml::IXMLDOMNodePtr pCol = pColsKey->nextNode();
        while (pCol != NULL) 
        {
            ossSQL << _T("`") << (LPCTSTR)pCol->text << _T("`");
            pCol = pColsKey->nextNode();
            if (pCol) ossSQL << _T(", ");
        }
    }

    ossSQL << _T(")");

    SmrtMsiHandle hView;
    OK(MsiDatabaseOpenView(m_db, ossSQL.str().c_str(), &hView));
    OK(MsiViewExecute(hView, NULL));
    OK(MsiViewClose(hView));

    m_currentTable.erase();
}

//------------------------------------------------------------------------------
// Populate table
//------------------------------------------------------------------------------
void Xml2Msi::populateTable(xml::IXMLDOMNode* table)
{
    _bstr_t bstrTable = table->attributes->getNamedItem(L"name")->nodeValue;
    m_currentTable = (LPCTSTR)bstrTable;

    // create the table view
    SmrtMsiHandle hView;
    tostringstream ossSQL;
    ossSQL << _T("SELECT * FROM `") << (LPCTSTR)bstrTable << _T("`");
    OK(MsiDatabaseOpenView(m_db, ossSQL.str().c_str(), &hView));
    OK(MsiViewExecute(hView, NULL));

    // copy column definitions into array
    std::vector<std::wstring> vecColDef;
    vecColDef.reserve(table->selectNodes(L"col")->length);
    xml::IXMLDOMNodeListPtr pColList(table->selectNodes(L"col"));
    xml::IXMLDOMNodePtr pCol;

    while ((pCol = pColList->nextNode()) != NULL) 
    {
        vecColDef.push_back((LPCWSTR)(_bstr_t)pCol->attributes->getNamedItem(L"def")->nodeValue);
    }

    // add rows
    xml::IXMLDOMNodeListPtr pRowList(table->selectNodes(L"row"));
    xml::IXMLDOMNodePtr pRow;
    while ((pRow = pRowList->nextNode()) != NULL) 
    {
        ++m_currentRow;
        m_currentCol = 0;

        // check column count
        xml::IXMLDOMNodeListPtr pTdList(pRow->selectNodes(L"td"));

        if ((size_t)pTdList->length != vecColDef.size()) 
        {
            tcerr << color::red << _T("Invalid number of <td> elements") << color::base << std::endl;
            _com_issue_error(E_FAIL);
        }

        // create record
        SmrtMsiHandle hRec(MsiCreateRecord(vecColDef.size()));
        if (hRec.isNull()) _com_issue_error(E_OUTOFMEMORY);
        OK(MsiRecordClearData(hRec));

        // populate record
        xml::IXMLDOMNodePtr pTd;
        while ((pTd = pTdList->nextNode()) != NULL) 
        {
            ++m_currentCol;

            // check if NULL field
            if (iswlower(vecColDef[m_currentCol-1][0]) 
                && pTd->hasChildNodes() == VARIANT_FALSE 
                && pTd->attributes->getNamedItem(L"href") == NULL) 
            {
                tcerr << color::red << _T("Field cannot be NULL") << color::base << std::endl;
                _com_issue_error(E_FAIL);
            }

            // case 1: text field
            if (towlower(vecColDef[m_currentCol-1][0]) != L'v') 
            {               
                _bstr_t bstrData(pTd->nodeTypedValue);
                OK(MsiRecordSetString(hRec, m_currentCol, (LPCTSTR)bstrData));
            }
            // case 2: binary stream   
            else            
            {
                // case 2a: local binary data (possibly encoded)
                if (pTd->attributes->getNamedItem(L"href") == NULL)
                {
                    if (pTd->hasChildNodes() == VARIANT_FALSE)
                        continue;

                    // access data w/o going through the wrapper functions
                    VARIANT var;
                    void* pArrayData = NULL;

                    try 
                    {
                        VariantInit(&var);
                        HRESULT hr = pTd->get_nodeTypedValue(&var);

                        if (FAILED(hr))
                            _com_issue_error(hr);

                        // we only support single-byte safe arrays
                        if (var.vt != (VT_ARRAY | VT_UI1)) 
                        {
                            tcerr << color::red << _T("Unsupported datatype") << color::base << std::endl;
                            _com_issue_error(E_FAIL);
                        }

                        //Get a safe pointer to the array
                        OK(SafeArrayAccessData(var.parray, &pArrayData));

                        // get size
                        long lLBound, lUBound, lElements;
                        OK(SafeArrayGetLBound(var.parray, 1, &lLBound));
                        OK(SafeArrayGetUBound(var.parray, 1, &lUBound));
                        lElements = lUBound - lLBound + 1;
                        checkMD5(pTd, pArrayData, lElements, sizeof(BYTE));

                        // copy data to temporary file
                        if (!m_tempPath.empty()) 
                        {
                            DeleteFile(m_tempPath.c_str()); // delete previous temporary file
                        }

                        _TCHAR strTmpDir[_MAX_PATH];
                        _TCHAR strTmpFile[_MAX_PATH];
                        GetTempPath(_MAX_PATH, strTmpDir);
                        GetTempFileName(strTmpDir, _T("bin"), 0, strTmpFile);
                        m_tempPath = strTmpFile;
                        SmrtFileHandle hFile(
                            CreateFile(m_tempPath.c_str(),
                            GENERIC_WRITE,
                            FILE_SHARE_READ,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN,
                            NULL));
                        if (hFile == INVALID_HANDLE_VALUE)
                            _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

                        DWORD dwLen;
                        if (WriteFile(hFile, (LPCSTR)pArrayData, lElements, &dwLen, NULL) == 0)
                            _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

                        // need to close hFile by hand, or MsiRecordSetStream won't accept it...
                        CloseHandle(hFile.release());

                        // feed to stream
                        OK(MsiRecordSetStream(hRec, m_currentCol, m_tempPath.c_str()));

                        // free VARIANT
                        OK(SafeArrayUnaccessData(var.parray));
                        OK(VariantClear(&var));
                    }
                    catch (...) 
                    {
                        if (pArrayData) OK(SafeArrayUnaccessData(var.parray));
                        OK(VariantClear(&var));
                        throw;
                    }
                }
                // case 2b: external binary data
                else if (pTd->hasChildNodes() == VARIANT_FALSE)
                {
                    xml::IXMLDOMNodePtr pHref(pTd->attributes->getNamedItem(L"href"));

                    // store data in local file
                    tstring filePath = resolveHref(pTd);

                    // check MD5
                    if (pTd->attributes->getNamedItem(L"md5") != NULL) 
                    {
                        // map file to memory
                        SmrtFileHandle hFile(
                            CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL));
                        if (hFile == INVALID_HANDLE_VALUE) _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

                        SmrtFileMap pMap;

                        if (GetFileSize(hFile, NULL) > 0)
                        {
                            // create file mapping
                            SmrtFileHandle hMap(CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL));
                            if (hMap == NULL) _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

                            // map file
                            pMap = SmrtFileMap(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0));
                            if (pMap.isNull()) _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
                        }

                        // check MD5
                        checkMD5(pTd, (LPCVOID)pMap, GetFileSize(hFile, NULL), sizeof(BYTE));
                    }

                    // feed to stream
                    OK(MsiRecordSetStream(hRec, m_currentCol, filePath.c_str()));
                }
                else 
                {
                    tcerr << color::red << _T("Field must be empty if href specified") << color::base << std::endl;
                    _com_issue_error(E_FAIL);
                }
            }
        }

        m_currentCol = 0;
        UINT res = MsiViewModify(hView, MSIMODIFY_INSERT, hRec);
        if (res == ERROR_FUNCTION_FAILED) 
        {
            tcerr << color::red << _T("The table contains non-unique primary keys") 
                << color::base << std::endl;
            _com_issue_error(E_FAIL);
        }
        OK (res);
    }

    m_currentRow = 0;
    OK(MsiViewClose(hView));
}

//------------------------------------------------------------------------------
//
// Resolve HREF
//
// Parameters:
//
//  pTd               - XMLDOMNode pointer to <td>
//
// Returns:
//
//  Local file path. If the href points to a local file, this
//  is the full path to the file. If the href points to an
//  external file, the path points to a temporary local file.
//  The temporary local file is deleted the next time resolveHref
//  is called, or when the application terminates.
//
//------------------------------------------------------------------------------
tstring Xml2Msi::resolveHref(xml::IXMLDOMNode* hrefNode)
{
    static _TCHAR szLocalPath[_MAX_PATH];

    // delete previous temporary file
    if (!m_tempPath.empty()) 
    {
        DeleteFile(m_tempPath.c_str());
    }

    // pointer to href attribute
    xml::IXMLDOMNodePtr pHref(hrefNode->attributes->getNamedItem(L"href"));

    try 
    {
        // build path name
        _tcscpy_s(szLocalPath, ARRAYSIZE(szLocalPath), (LPCTSTR)(_bstr_t)pHref->nodeValue);
        if (_tcscspn(szLocalPath, _T(":")) == _tcslen(szLocalPath)) 
        {
            _TCHAR drive[_MAX_DRIVE];
            _TCHAR dir[_MAX_DIR];
            _TCHAR fname[_MAX_FNAME];
            _TCHAR ext[_MAX_EXT];
            _tsplitpath_s(szLocalPath, 
                          drive, ARRAYSIZE(drive), 
                          dir, ARRAYSIZE(dir), 
                          fname, ARRAYSIZE(fname), 
                          ext, ARRAYSIZE(ext));
            if (drive[0] == _T('\0') && dir[0] != _T('\\') && m_hrefPrefix[0] != _T('\0')) 
            {
                // prepend path prefix
                _TCHAR dir2[_MAX_DIR];
                _TCHAR buf[_MAX_PATH];
                _tcscpy_s(buf, ARRAYSIZE(buf), m_hrefPrefix.c_str());
                _tcscat_s(buf, ARRAYSIZE(buf), _T("\\"));
                _tsplitpath_s(buf, drive, ARRAYSIZE(drive), dir2, ARRAYSIZE(dir2), NULL, 0, NULL, 0);
                _tcscat_s(dir2, ARRAYSIZE(dir2), dir);
                _tmakepath_s(szLocalPath, ARRAYSIZE(szLocalPath), drive, dir2, fname, ext);
            }
        }

        URL_COMPONENTS urlComp;
        ZeroMemory((PVOID)&urlComp, sizeof(URL_COMPONENTS));
        urlComp.dwStructSize = sizeof(URL_COMPONENTS);
        urlComp.dwSchemeLength = 1;
        urlComp.dwUrlPathLength = 1;

        _TCHAR strUrl[1024];
        DWORD dwLen;
        if (!InternetCombineUrl((LPCTSTR)(_bstr_t)pHref->ownerDocument->url, szLocalPath,
                                strUrl, &(dwLen = ARRAYSIZE(strUrl)), ICU_DECODE) ||
            !InternetCrackUrl(strUrl, 0, 0, &urlComp)) 
        {
            _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
        }

        if (urlComp.nScheme == INTERNET_SCHEME_FILE)
        {
            // decode file path
            InternetCombineUrl((LPCTSTR)(_bstr_t)pHref->ownerDocument->url, szLocalPath,
                strUrl, &(dwLen = ARRAYSIZE(strUrl)), ICU_DECODE|ICU_NO_ENCODE|ICU_NO_META);

            // check if the file is accessible
            SmrtFileHandle hFile(
                CreateFile(urlComp.lpszUrlPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL));
            if (hFile == INVALID_HANDLE_VALUE)
                _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
            return urlComp.lpszUrlPath;
        }
        else if (urlComp.nScheme != INTERNET_SCHEME_UNKNOWN)
        {
            // need to download to temporary file
            _TCHAR strTempDir[_MAX_PATH];
            _TCHAR strTempFile[_MAX_PATH];
            GetTempPath(_MAX_PATH, strTempDir);
            GetTempFileName(strTempDir, _T("bin"), 0, strTempFile);
            m_tempPath = strTempFile;
            SmrtFileHandle hFile(
                CreateFile(m_tempPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN, NULL));

            if (hFile == INVALID_HANDLE_VALUE)
                _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

            // open internet connection
            SmrtHInternet hInet(
                InternetOpen(_T("xml2msi"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
            if (hInet.isNull()) _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

            SmrtHInternet hInetFile(
                InternetOpenUrl(hInet, strUrl, NULL, 0, INTERNET_FLAG_EXISTING_CONNECT, 0));
            if (hInetFile.isNull()) _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));


            // Read data
            for (;;) 
            {
                BYTE buf[1024];
                DWORD dwLen = sizeof(buf);
                if (!InternetReadFile(hInetFile, (LPVOID)buf, dwLen, &dwLen))
                    _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
                if (dwLen == 0) break;

                // save to temporary file
                if (WriteFile(hFile, (LPVOID)buf, dwLen, &dwLen, NULL) == 0)
                    _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
            }

            return m_tempPath.c_str();
        }
        else
        {
            if (_tcsncmp(urlComp.lpszScheme, _T("media:"), 6) != 0)
                _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

            // decode file path
            InternetCombineUrl((LPCTSTR)(_bstr_t)pHref->ownerDocument->url, szLocalPath,
                strUrl, &(dwLen = ARRAYSIZE(strUrl)), ICU_DECODE|ICU_NO_ENCODE|ICU_NO_META);

            _bstr_t dir(m_tempCabDir.c_str());
            dir += urlComp.lpszUrlPath;

            SmrtFileHandle hFile(
                CreateFile(dir, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN, NULL));

            if (hFile == INVALID_HANDLE_VALUE)
                _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

            return (LPCTSTR)dir;
        }
    }
    catch (...) 
    {
        tcerr << color::red << _T("Invalid href to ") << (LPCTSTR)(_bstr_t)pHref->nodeValue << color::base << std::endl;
        throw;
    }
}


//------------------------------------------------------------------------------
//
// Format SQL column specification
//
//------------------------------------------------------------------------------
tstring Xml2Msi::buildSQLColSpec(xml::IXMLDOMNode* columnNode)
{
    tostringstream ossSQL;

    if (columnNode->attributes->getNamedItem(L"def") == NULL) 
    {
        tcerr << color::red << _T("Missing column definition for column \"") << (LPCTSTR)columnNode->text << _T("\"") << color::base << std::endl;
        _com_issue_error(E_FAIL);
    }

    ossSQL << _T("`") << (LPCTSTR)columnNode->text << _T("` ");

    tstring str((LPCTSTR)(bstr_t)columnNode->attributes->getNamedItem(L"def")->nodeValue);

    int lLen;
    tistringstream iss(str);
    iss.ignore(1) >> lLen;

    switch (tolower(str[0])) 
    {
    case 's':  // string
        // fall through !
    case 'l':  // string (localizable)
        if (lLen == 0) 
            ossSQL << _T("LONGCHAR");
        else if (lLen > 0 && lLen < 256)
            ossSQL << _T("CHAR(") << lLen << _T(")");
        else 
        {
            tcerr << color::red << _T("Invalid string length specified: ") << str << color::base << std::endl;
            _com_issue_error(E_FAIL);
        }
        break;

    case 'i':  // integer
        if (lLen == 2)
            ossSQL << _T("SHORT");
        else if (lLen == 4)
            ossSQL << _T("LONG");
        else 
        {
            tcerr << color::red << _T("Invalid integer size specified: ") << str << color::base << std::endl;
            _com_issue_error(E_FAIL);
        }
        break;

    case 'v':  // binary stream
        if (lLen != 0) 
        {
            tcerr << color::red << _T("Error in binary stream specification: ") << str << color::base << std::endl;
            _com_issue_error(E_FAIL);
        }
        ossSQL << _T("OBJECT");
        break;;

    default:
        tcerr << color::red << _T("Invalid field definition: ") << str << color::base << std::endl;
        _com_issue_error(E_FAIL);
        break;
    }

    if (_istlower(str[0]))
        ossSQL << _T(" NOT NULL");

    if (tolower(str[0]) == 'l')
        ossSQL << _T(" LOCALIZABLE");

    return ossSQL.str();
}

//------------------------------------------------------------------------------
//
// Create summary info stream
//
//------------------------------------------------------------------------------
void Xml2Msi::createSummaryInfo()
{
    // summary information tags
    struct SumInfo 
    {
        LPCTSTR szTag;
        INT nType;
    };

    static const SumInfo szSumInfoTags[19] = 
    {
        { _T("codepage"), VT_I2 }, 
        { _T("title"), VT_LPSTR },
        { _T("subject"), VT_LPSTR }, 
        { _T("author"), VT_LPSTR},
        { _T("keywords"), VT_LPSTR }, 
        { _T("comments"), VT_LPSTR },
        { _T("template"), VT_LPSTR }, 
        { _T("lastauthor"), VT_LPSTR },
        { _T("revnumber"), VT_LPSTR }, 
        { _T("null"), VT_NULL },
        { _T("lastprinted"), VT_FILETIME }, 
        { _T("createdtm"), VT_FILETIME },
        { _T("lastsavedtm"), VT_FILETIME }, 
        { _T("pagecount"), VT_I4 },
        { _T("wordcount"), VT_I4 }, 
        { _T("charcount"), VT_I4 },
        { _T("null"), VT_NULL }, 
        { _T("appname"), VT_LPSTR },
        { _T("security"), VT_I4 }
    };

    m_currentTable.erase();

    SmrtMsiHandle hSumInfo;
    OK(MsiGetSummaryInformation(m_db, NULL, 17, &hSumInfo));

    xml::IXMLDOMNodeListPtr pSumInfoNodes(m_doc->selectNodes(L"/msi/summary/*"));
    xml::IXMLDOMNodePtr pSumInfoNode;

    while ((pSumInfoNode = pSumInfoNodes->nextNode()) != NULL) 
    {
        UINT i;

        for (i = 1; i < 20; ++i) 
        {
            if (pSumInfoNode->nodeName == _bstr_t(szSumInfoTags[i - 1].szTag))
                break;
        }

        if (i == 20) 
        {
            tcerr << color::red << _T("Invalid summary info element: \"") << (LPCTSTR)pSumInfoNode->nodeName << _T("\"") << color::base << std::endl;
            _com_issue_error(E_FAIL);
        }

        if (pSumInfoNode->text == _bstr_t(""))
            continue;

        switch (szSumInfoTags[i - 1].nType) 
        {
        case VT_I2:        // 2 byte signed int
            // fall through !
        case VT_I4:        // 4 byte signed int
            OK(MsiSummaryInfoSetProperty(hSumInfo, i, szSumInfoTags[i - 1].nType,
                _ttoi((LPCTSTR)pSumInfoNode->text),
                NULL, NULL));
            break;

        case VT_LPSTR:     // null terminated string
            OK(MsiSummaryInfoSetProperty(hSumInfo, i, szSumInfoTags[i - 1].nType,
                NULL, NULL, (LPCTSTR)pSumInfoNode->text));
            break;

        case VT_FILETIME:  // FILETIME
            {
                FILETIME ftLocal, ftValue;
                SYSTEMTIME systime;
                ZeroMemory(&systime, sizeof(systime));

                if (_stscanf_s((LPTSTR)pSumInfoNode->text,
                             _T("%hd/%hd/%hd %hd:%hd"),
                             &systime.wMonth,
                             &systime.wDay,
                             &systime.wYear,
                             &systime.wHour,
                             &systime.wMinute) != 5) 
                {
                    tcerr << color::red << _T("Invalid date/time in summary info: \"") 
                          << (LPCTSTR)pSumInfoNode->nodeName
                          << _T("\"") << color::base << std::endl;
                    _com_issue_error(E_FAIL);
                }

                if (!SystemTimeToFileTime(&systime, &ftLocal) ||
                    !LocalFileTimeToFileTime(&ftLocal, &ftValue)) 
                {
                    tcerr << color::red << _T("Invalid date/time in summary info: \"") 
                          << (LPCTSTR)pSumInfoNode->nodeName
                          << _T("\"") << color::base << std::endl;
                    _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));
                }

                OK(MsiSummaryInfoSetProperty(hSumInfo, i, szSumInfoTags[i - 1].nType,
                    NULL, &ftValue, NULL));
            }

            break;

        default:
            _com_issue_error(E_FAIL);
        }
    }

    OK(MsiSummaryInfoPersist(hSumInfo));
}

//------------------------------------------------------------------------------
//
// Check MD5 checksum
//
// Parameters:
//
//  pData             - Pointer to data
//
//  nLen              - Length of data.
//
//  pTd               - XMLDOMNode pointer to <td> element
//
//  m_currentRow, m_currentCol         - location of node for error reporting
//
//------------------------------------------------------------------------------
void Xml2Msi::checkMD5(xml::IXMLDOMNode* md5Node, const void* data, int len, int size)
{
    if (md5Node->attributes->getNamedItem(L"md5") == NULL)
        return ;

    MD5_CTX ctx;
    MD5Init(&ctx);
    MD5Update(&ctx, data, len, size);
    MD5Final(&ctx);

    _bstr_t bstrMD5(md5Node->attributes->getNamedItem(L"md5")->nodeValue);

    _TCHAR szCtx[33];
    int j;
    for (j = 0; j < 16; ++j) 
    {
        _stprintf_s(szCtx + 2 * j, ARRAYSIZE(szCtx) - 2 * j, _T("%02x"), ctx.digest[j]);
    }

    szCtx[2*j] = _T('\0');
    if (_tcsicmp(szCtx, (LPCTSTR)bstrMD5) != 0) 
    {
        _bstr_t bstrTable(md5Node->parentNode->parentNode->attributes->getNamedItem(L"name")->nodeValue);

        if (m_checkMD5) 
        {
            tcerr << color::red << _T("Failed MD5 checksum test") << std::endl;
            tcerr << _T("New checksum: ") << szCtx << std::endl;
            tcerr << _T("(Use the -m option to ignore this error)") << color::base << std::endl;
            _com_issue_error(E_FAIL);
        }
        else
        {
            // update value
            md5Node->attributes->getNamedItem(L"md5")->nodeValue = szCtx;

            if (!m_quiet)
            {
                tcerr << color::yellow << _T("Warning: Failed MD5 checksum test in table ")
                      << _T("\"") << m_currentTable << _T("\"")
                      << _T(", row ") << m_currentRow 
                      << _T(", field ") << m_currentCol << color::base << std::endl;
            }
        }
    }
}

//------------------------------------------------------------------------------
// Print banner message
//------------------------------------------------------------------------------
void Xml2Msi::printBanner() const
{
    if (!m_nologo)
    {
        tcerr << _T("xml2msi ") << moduleVersion();
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
void Xml2Msi::printUsage()
{
    tcerr << _T("\nUsage: xml2msi [-m] [-p PREFIX] [-c [GUID]] [-d [GUID]] [-e] [-g [GUID]]") << std::endl;
    tcerr << _T("               [-v VERSION] [-r [VERSION]] [-u [XMLFILE]] [-o MSIFILE] XMLFILE") << std::endl;
    tcerr << _T(" -Q --nologo               don't print banner message") << std::endl;
    tcerr << _T(" -q --quiet                quiet processing") << std::endl;
    tcerr << _T(" -m --ignore-md5           treat failed MD5 checks as warnings") << std::endl;
    tcerr << _T(" -p --href-prefix=PREFIX   prepend PREFIX to external references (href)") << std::endl;
    tcerr << _T(" -c --package-code=GUID    update package code (with GUID)") << std::endl;
    tcerr << _T(" -d --product-code=GUID    update product code (with GUID)") << std::endl;
    tcerr << _T(" -e --component-code       update component codes") << std::endl;
    tcerr << _T(" -g --upgrade-code=GUID    update upgrade code (with GUID)") << std::endl;
    tcerr << _T(" -v --product-version=VER  update product version with VER") << std::endl;
    tcerr << _T(" -r --upgrade-version=VER  update 'Upgrade' table entry 'VersionMax'") << std::endl;
    tcerr << _T(" -u --update-xml=FILE      write updated XML to FILE") << std::endl;
    tcerr << _T(" -s --set=\"property=value\" set/update property to 'value'") << std::endl;
    tcerr << _T(" -o --output=FILE          write MSI file to FILE") << std::endl;
    tcerr << std::endl;
}

//------------------------------------------------------------------------------
// Parse command line
//------------------------------------------------------------------------------
void Xml2Msi::parseCommandLine(int argc, _TCHAR* argv[])
{
    _TCHAR drive[_MAX_DRIVE];
    _TCHAR dir[_MAX_DIR];
    _TCHAR fname[_MAX_FNAME];
    _TCHAR ext[_MAX_EXT];

    // short option string (option letters followed by a colon ':' require an argument)
    static const _TCHAR optstring[] = _T("lqQmp:o:u:c:d:ev:g:r:s:");

    // mapping of long to short arguments
    static const Option longopts[] = 
    {
        // general options
        { _T("license"),            no_argument,        NULL,   _T('l') },
        { _T("quiet"),              no_argument,        NULL,   _T('q') },
        { _T("nologo"),             no_argument,        NULL,   _T('Q') },
        { _T("ignore-md5"),         no_argument,        NULL,   _T('m') },
        { _T("href-prefix"),        required_argument,  NULL,   _T('p') },
        { _T("update-xml"),         optional_argument,  NULL,   _T('u') },
        { _T("package-code"),       optional_argument,  NULL,   _T('c') },
        { _T("product-code"),       optional_argument,  NULL,   _T('d') },
        { _T("component-code"),     no_argument,        NULL,   _T('e') },
        { _T("upgrade-code"),       optional_argument,  NULL,   _T('g') },
        { _T("product-version"),    required_argument,  NULL,   _T('v') },
        { _T("upgrade-version"),    required_argument,  NULL,   _T('r') },
        { _T("set"),                required_argument,  NULL,   _T('s') },
        { _T("output"),             required_argument,  NULL,   _T('o') },
        { NULL,                     0,                  NULL,   0       }
    };

    int longIdx = 0;
    int c;
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

        case _T('m'):  // ignore MD5 checksum
            m_checkMD5 = false;
            break;

        case _T('p'):  // path prefix
            if (optarg) 
            {
                m_hrefPrefix = optarg;
            }
            break;

        case _T('o'):  // out file
            if (optarg) 
            {
                m_outputPath = optarg;
            }
            break;

        case _T('u'): // xml output file
            m_udpateXml = true;
            if (optarg) m_xmlOutputPath = optarg;
            break;

        case _T('c'): // update package code
            if (optarg)
            {
                m_packageCode = optarg;
            }
            else
            {
                m_packageCode = guid();
            }
            break;

        case _T('d'): // update product code
            if (optarg)
            {
                m_productCode = optarg;
            }
            else
            {
                m_productCode = guid();
            }
            break;

        case _T('e'): // update product code
            m_componentCode = true;
            break;

        case _T('g'): // update upgrade code
            if (optarg)
            {
                m_upgradeCode = optarg;
            }
            else
            {
                m_upgradeCode = guid();
            }
            break;

        case _T('v'): // product version
            if (optarg)
            {
                m_productVersion = parseVersion(optarg);
            }
            else
            {
                printBanner();
                tcerr << color::red << _T("Missing required argument for product version") << color::base << std::endl << std::endl;
                printUsage();
                exit(2);
            }
            break;

        case _T('r'): // update upgrade version
            m_updateUpgradeVersion = true;
            if (optarg)
            {
                m_upgradeVersion = parseVersion(optarg);
            }
            break;

        case _T('s'): // set property
            if (optarg)
            {
                _TCHAR* nextToken;
                if (_TCHAR* property = _tcstok_s(optarg, _T("="), &nextToken))
                {
                    if (_TCHAR* value = _tcstok_s(NULL, _T("="), &nextToken))
                    {
                        m_propertyMap[property] = value;
                    }
                    else
                    {
                        m_propertyMap[property] = _T("");
                    }
                }
            }
            break;

        case _T('l'):  // show license and exit
            printBanner();
            std::cerr << loadTextResource(IDR_GPL);
            exit(0);

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

    if (argv[optind]) m_inputPath = argv[optind];

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
    else if (!m_inputPath.empty())
    {
        // create from input file
        _tsplitpath_s(m_inputPath.c_str(), 
                      drive, ARRAYSIZE(drive), 
                      dir, ARRAYSIZE(dir), 
                      fname, ARRAYSIZE(fname), 
                      ext, ARRAYSIZE(ext));
        _tmakepath_s(buf, ARRAYSIZE(buf), NULL, currentDir.c_str(), fname, _T("MSI"));
        m_outputPath = buf;
        m_fixExtension = true;
    }

    // determine XML output folder
    if (!m_xmlOutputPath.empty()) 
    {
        _tsplitpath_s(m_xmlOutputPath.c_str(), 
                      drive, ARRAYSIZE(drive), 
                      dir, ARRAYSIZE(dir), 
                      fname, ARRAYSIZE(fname), 
                      ext, ARRAYSIZE(ext));
        if (drive[0] == '\0' && dir[0] != '\\') 
        {
            m_xmlOutputPath = currentDir + m_xmlOutputPath;
        }
    }
    else if (!m_inputPath.empty())
    {
        // create from input file
        m_xmlOutputPath = m_inputPath;
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
}

//------------------------------------------------------------------------------
tstring Xml2Msi::moduleVersion()
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
tstring Xml2Msi::parseVersion(LPCTSTR argument)
{
    _TCHAR drive[_MAX_DRIVE];
    _TCHAR dir[_MAX_DIR];
    _TCHAR fname[_MAX_FNAME];
    _TCHAR ext[_MAX_EXT];

    // check if a regular version
    unsigned a, b, c, d;
    a = b = c = d = 0;
    if (_stscanf_s(argument, _T("%u.%u.%u.%u"), &a, &b, &c, &d) >= 3)
    {
        return argument;
    }

    tstring versionFile(argument);
    _tsplitpath_s(versionFile.c_str(), 
                  drive, ARRAYSIZE(drive), 
                  dir, ARRAYSIZE(dir), 
                  fname, ARRAYSIZE(fname), 
                  ext, ARRAYSIZE(ext));
    if (drive[0] == '\0' && dir[0] != '\\') 
    {
        // get current directory
        _TCHAR currentDir[MAX_PATH+1];
        GetCurrentDirectory(MAX_PATH, currentDir);
        versionFile = currentDir + tstring(_T("\\")) + versionFile;
    }
    
    DWORD dw; 
    DWORD len = GetFileVersionInfoSize(const_cast<LPTSTR>(versionFile.c_str()), &dw);
    if (len == 0)
    {
        tcerr << color::red << _T("Invalid version argument '") << argument << _T("'") << color::base << std::endl;
        exit(2);
    }

    std::vector<_TCHAR> buf(len);
    GetFileVersionInfo(const_cast<LPTSTR>(versionFile.c_str()), 0, len, &buf[0]);
    unsigned int vlen;
    LPVOID lpvi;
    VerQueryValue(&buf[0], _T("\\"), &lpvi, &vlen);

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
//
// Load text resource
//
// Parameters:
//
//  idRes             - resource ID
//
// Returns:
//
//  pointer to text array
//
//------------------------------------------------------------------------------
std::string Xml2Msi::loadTextResource(WORD idRes)
{
    HRSRC hXMLTempl = FindResource(NULL, MAKEINTRESOURCE(idRes), _T("TEXT"));

    if (hXMLTempl == NULL)
        _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

    HGLOBAL hResXMLTempl = LoadResource(NULL, hXMLTempl);

    if (hResXMLTempl == NULL)
        _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

    LPCSTR szXMLTempl = (LPCSTR)LockResource(hResXMLTempl);

    if (!szXMLTempl )
        _com_issue_error(HRESULT_FROM_WIN32(GetLastError()));

    DWORD len = SizeofResource(NULL, hXMLTempl);

    return std::string(szXMLTempl, len);
}

//------------------------------------------------------------------------------
tstring Xml2Msi::guid()
{
    UUID uuid;
    _TUCHAR* uuidString;

    UuidCreate(&uuid);
	UuidToString(&uuid, (RPC_WSTR*)&uuidString);
    tstring result = tstring(_T("{")) + uuidString + _T("}");
    std::transform(result.begin(), result.end(), result.begin(), std::ptr_fun(toupper));
	RpcStringFree((RPC_WSTR*)&uuidString);

    return result;
}

//------------------------------------------------------------------------------
int Xml2Msi::nodeValue(xml::IXMLDOMNodePtr& node)
{   
    HRESULT hr;
    VARIANT var;
    int value = 0;

    VariantInit(&var);
    hr = node->get_nodeTypedValue(&var);
    if (SUCCEEDED(hr))
    {
        hr = VariantChangeType(&var, &var, 0, VT_I4);
        if (SUCCEEDED(hr))
        {
            value = var.lVal;
        }
    }
    VariantClear(&var);
    return value;
}


