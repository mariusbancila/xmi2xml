# xmi2xml

A Windows Installer Database To XML Bi-Directional Converter

----

**This project is a fork of the [MSI2XML](https://msi2xml.sourceforge.io/) project by *Daniel Gehriger* from [SourceForge](https://sourceforge.net/projects/msi2xml/) (which has not been developed since 2007).**

----

The command line tool **msi2xml** converts Windows Installer Databases (`.msi` / `.msm`) to text based XML files.
The complementary tool **xml2msi** reconstructs the `.msi` / `.msm` from the XML file.

There are several possible uses for msi2xml/xml2msi:

- **Quality assurance**: use the human readable XML file to compare changes between different versions of a .msi file.
- **Version control**: many version control systems are text based. Store the XML file instead of the .msi file.
- **Automated build systems**: using xml2msi, existing installations may be updated with new files, and automatically rebuilt.

## Features 

### msi2xml

- Includes binary streams as MIME (base64) encoded text in the XML file.
- Alternatively writes binary streams as external files and includes them by references in the XML file.
- Ensures integrity of binary streams by including a MD5 fingerprint.
- Includes a default style sheet to display the XML file as a web page in Internet Explorer 5.
- Sorts rows according to key columns.
- Version control friendly: time stamps of binary files only change when the file's content changes.

## xml2msi

- Instead of including field data in the XML file, an external reference may be specified instead (href). Both, URLs and local paths are resolved dynamically.
- Accepts an MD5 checksum for all data fields, to ensure data integrity.
- Automatically sets the correct column types for the standard tables.
- Validates the data types for standard table columns.

## Installation
To install `msi2xml` download the Windows Installer Package (.msi).

As of release 2.2.0 **msi2xml** requires MSXML 6.0 to be installed on your system. MSXML 6.0 is pre-installed on Windows Vista and included with the .NET Framework 2.0. If the the **msi2xml** setup detects that MSXML 6.0 is missing, you can [download](http://www.microsoft.com/downloads/results.aspx?pocId=&freetext=msxml6.msi&DisplayLang=en) it from the [Microsoft web site](http://www.microsoft.com/downloads/results.aspx?pocId=&freetext=msxml6.msi&DisplayLang=en).

## Usage of msi2xml

```
msi2xml [-q] [-n] [-m] [-e ENCODING] [-s [STYLESHEET]] [-b [DIR]] [-c [DIR[,MEDIACABS]] [-o XMLFILE] file

-q --quiet                    quiet processing
-n --no-sort                  disable sorting of rows
-m --merge-module             convert a merge module (.msm)
-e --encoding=ENCODING        force XML encoding to ENCODING (default is US-ASCII)
-s --stylesheet               disable default XSL stylesheet
-s --stylesheet=NAME          use XSL stylesheet NAME
-b --dump-streams=DIR         save binary streams to DIR subdirectory
-c --extract-cabs=DIR,MEDIAS  extract content of cabinet files of MEDIAS to DIR (see notes)
-o --output=FILE              write MSI file to FILE
```

**Note:**

- To allow for easier comparing, rows are sorted according to the first field's content. xml2msi will take up to 20 seconds to convert a `.msi` file if row sorting is enabled. To disable row sorting, and to speed up the conversion, add the `-n` option.
- To convert a merge module, add the `-m` switch. This also sets the merge module attribute in the XML file to `yes`, and xml2msi automatically reconstructs a merge module from it.
- The `-c` / `--extract-cabs` option takes either no argument, a single argument or a comma-separated list of arguments:
  - **No argument**: all cabinet files listed in the Media table are extracted to the same directory as the output XML file;
  - **A single argument** (eg. `--extract-cabs=cabs`): all cabinet files listed in the Media table are extracted to the cabs sub-folder of the folder containing the output XML file;
  - **Comma-separated list of arguments** (eg: `--extract-cabs=cabs,Cabs.1.cab,Cabs.2.cab`): only the cabinet files `Cabs.1.cab` and `Cabs.2.cab` are extracted to the cabs sub-folder. Use a dot for the output folder to specify the default output folder: (eg: `--extract-cabs=.,Cabs.1.cab,Cabs.2.cab`)

**Examples:**

To convert a MSI file to XML use the following command:

```
msi2xml installation.msi
```

This creates the XML file `installation.xml` and the default stylesheet `msi.xsl`.

To change the name of the generated XML file, use the `-o` option:

```
msi2xml -o outname.xml installation.msi
```

To extract all binary streams to external files and also decompress all cabinet files, use:

```
msi2xml -b streams -c files installation.msi
```

## Usage of xml2msi

```
xml2msi [-m] [-p PREFIX] [-c [GUID]] [-d [GUID]] [-g [GUID]] 
    [-v VERSION] [-r [VERSION]] [-u [XMLFILE]] [-s "PROPERTY=  VALUE"] [-o MSIFILE] XMLFILE

-q --quiet                 quiet processing
-m --ignore-md5            treat failed MD5 checks as warnings
-p --href-prefix=PREFIX    prepend PREFIX to external references (href)
-c --package-code=GUID     update package code (with GUID, see notes)
-d --product-code=GUID     update product code (with GUID, see notes)
-g --upgrade-code=GUID     update upgrade code (with GUID, see notes)
-v --product-version=VER   update product version with VER (see notes)
-r --upgrade-version=VER   update 'Upgrade' table entry 'VersionMax' (uses product version if VER omited, also see notes)
-u --update-xml=FILE       write updated XML to FILE (or update input file if FILE omited)

-s --set="PROPERTY=VALUE"  set/update PROPERTY in Property table to VALUE (repeat option for setting multiple properties)
-o --output=FILE           write MSI file to FILE
```

**Notes:**

- If the optional GUID argument is omitted, **xml2msi** creates a new GUID and uses it as the argument
- The version arguments VER can either be an explicit version (`1.2.3.4`) or the path (absolute or relative to current directory) to a file. **xml2msi** will extract the file version of this file and use the result as the argument to the option.

**Examples:**

To build a MSI file from an XML file use the following command:

```
xml2msi installation.xml
```

This creates the MSI file `installation.msi`.

If some of the MD5 checksum tests fail, due to changed binary files, specify the -m option to force the creation of the MSI file:

```
xml2msi -m installation.xml
```

The following command line is used to distribute a new version of the msi2xml package (notes in blue):

```
xml2msi    --package-code		; stamps package code with a new GUID
    --product-code			; stamps product code with a new GUID
    --product-version=msi2xml.exe	; updates the product version with the file version of the file "msi2xml.exe"
    --upgrade-version		; updates the VersionMax entry of Upgrade table to the new product version
    --update-xml			; writes modifications back to input XML
    msi2xml.xml			; input XML file (output is written to msi2xml.MSI)
```

## Anatomy of the XML file

The XML DOCTYPE is given in the next chapter. The following show the typical structure of a MSI-XML file:

```
<?xml version="1.0"?>
<msi version="2.0" codepage="932" xmlns:dt="urn:schemas-microsoft-com:datatypes">

   <summary>
       <codepage>Codepage</codepage>
       <title>Title</title>
            .
            .
       <security>Security</security>
   </summary>

   <table name="TableName">
        <col key="yes" def="s72">Col1</col>
        <col key="no" def="v0">Col2</col>

        <row>
            <td>Field1</td>
            <td dt:dt="bin.base64">Binary Encoded Data</td>
        </row>
            .
            .
         <row>
            <td>Field1</td>
            <td href="./bin/file" />
        </row>
      
    </table>
</msi>
```

**Notes:**

1. The file starts with the XML processing instruction: `<?xml version="1.0"?>`

2. The database tags are enclosed by a `<msi> ... </msi>` tag pair.

2.1 The `<msi>` tag carries a mandatory version specifier, which must be "2.0".

2.2 You may optionally specify a codepage value using the "codepage" attribute.

2.3 Merge modules have the "msm" attribute set to "yes".

2.4 If you are using Base64-encoded binary data, the following namespace must be declared: xmlns:dt="urn:schemas-microsoft-com:datatypes"

2.5 The optional compression attribute specifies the algorithm used to compress files into cabinet files.

3. A `<summary> ... </summary>` declares the Summary Stream Information (see MSI doc).

3.1 Each Summary Stream Information Property is declared in a tag pair carrying the property's name.

4. The rest of the file consists of a series of `<table> ... </table>` blocks to define the database tables.

4.1 The table name is given by the "name" attribute of the `<table>` tag.

5. The table columns must be declared by a series of `<col> ... </col>` tags, each containing the table name.

5.1 The "key" attribute must be "yes" for primary key columns.

5.2 The column datatype must be declared in the "def" attribute. The syntax of the "def" attribute follows the Column Definition Format syntax, as specified in the MSI documentation.

5.3 Neither, the "key" and the "def" attribute are required for the standard database tables. xml2msi will use valid defaults if they are missing.

6. Each table record is enclosed in a `<row> ... </row>` block.

7. Each `<row> ... </row>` block contains a series of field tags `<td>`.

7.1 The number of `<td>` fields must correspond to the number of declared columns (`<col>`).

7.2 The field content is specified between the `<td> ... </td>` tags.

7.3 White-space (including line breaks) are preserved.

7.4 The characters `&`, `<` and `>` must be escaped as `&amp;`, `&lt;` and `&gt;`.Alternatively, the data may be enclosed in a CDATA section:
```
<td><![CDATA[.............]]></td>
```
All characters, except the sequence `]]` are allowed within a `CDATA` section.

7.5 A `NULL` field is specified as `<td></td>`, or shorter: `<td/>`

7.6 Field content, especially binary data, may be encoded as Base64 MIME data. In this case, the attribute "dt:dt" must be specified as follows:
```
<td dt:dt="bin.base64"> .... base64 encoded data .... </td>
```

7.7 Instead of specifying the binary field content in the XML file, an external reference may be specified using the "href" attribute:
```
<td href="./bin/file.cab" />
```
Both, Internet URLs and local (relative & absolute) paths may be specified.

7.8 If you specify the special protocol "media:" in a href attribute of a binary field, xml2msi will build a cabinet file of the specified media and insert it on the fly. Example: href="media:Cabs.w1.cab" looks up the media "Cabs.w1.cab" in the media table and builds the cabinet before inserting it in the binary field.

7.9 An optional MD5 checksum may be included in the "md5" attribute:
```
<td md5="...checksum..."> </td>
```
xml2msi validates the checksum for both, locally specified and external (href) field data.

## MSI-XML DOCTYPE

```
<!ELEMENT msi (summary,table*)>
<!ATTLIST msi version CDATA #REQUIRED>
          xmlns:dt CDATA #IMPLIED
      codepage CDATA #IMPLIED
      msm      (yes|no) "no"
      compression (MSZIP|LZX|none) "LZX">

<!ELEMENT summary (codepage?,title?,subject?,author?,keywords?,comments?,
    template,lastauthor?,revnumber,lastprinted?,
    createdtm?,lastsavedtm?,pagecount,wordcount,
    charcount?,appname?,security?)>

<!ELEMENT codepage (#PCDATA)>
<!ELEMENT title (#PCDATA)>
<!ELEMENT subject (#PCDATA)>
<!ELEMENT author (#PCDATA)>
<!ELEMENT keywords (#PCDATA)>
<!ELEMENT comments (#PCDATA)>
<!ELEMENT template (#PCDATA)>
<!ELEMENT lastauthor (#PCDATA)>
<!ELEMENT revnumber (#PCDATA)>
<!ELEMENT lastprinted (#PCDATA)>
<!ELEMENT createdtm (#PCDATA)>
<!ELEMENT lastsavedtm (#PCDATA)>
<!ELEMENT pagecount (#PCDATA)>
<!ELEMENT wordcount (#PCDATA)>
<!ELEMENT charcount (#PCDATA)>
<!ELEMENT appname (#PCDATA)>
<!ELEMENT security (#PCDATA)> 

<!ELEMENT table (col+,row*)>
<!ATTLIST table
    name CDATA #REQUIRED>

<!ELEMENT col (#PCDATA)>
<!ATTLIST col
    key (yes|no) #IMPLIED
    def CDATA #IMPLIED>

<!ELEMENT row (td+)>

<!ELEMENT td (#PCDATA)>
<!ATTLIST td
    href CDATA #IMPLIED
    dt:dt (string|bin.base64) #IMPLIED
    md5 CDATA #IMPLIED>
```

## Revision History

#### 04-September-2024 - Release 2.3.0
- Fixed bug: unhandled exception when unpacking large `.msi` files due to failed memory allocation

### Old revision history from SourceForce

#### 27-September-2007 - Release 2.2.0
- [ #1509709 ] Using MSXML 6.0 or MSXML 6.0 SP 1 (pre-installed on Windows Vista and included with .NET Framework 2.0; otherwise download from here.)
- Fixed bug [ #1469822 ] Invalid XML characters are base-64 encoded
- Coloring error / warning / notice / status messages in red / yellow / green / white
- Fixed bug [ Forum ] "The table contains non-unique primary keys"
- Compatible with Internet Explorer 7
- Updated to VisualC++ 2005 (VC8)
- Fixed resource loading    
- Icorporated patch [ #1406769 ] Improved save to disk in XML
- Fixed bug [ #1469818 ] Runtime error when missing external CAB encountered
- Fixed bug [ #907971 ] Continuing after MSI errors when dumping tables

#### 18-September-2005 - Release 2.1.11
- Added -Q switch to suppress banner

#### 15-August-2005 - Release 2.1.9
- added switch to set / update values in Property table (see new switch -s in xml2msi)

#### 16-February-2004 - Release 2.1.8
- added merge module support (see new switch -m in msi2xml)

#### 11-December-2003 - Release 2.1.7
- fixed date formatting error resulting in "Failed to convert the MSI database!" error message

#### 9-December-2003 - Release 2.1.6
- fixed error message when empty node encountered

#### 11-November-2002 - Release 2.1.5
- fixed decompression of zero-size files
- fixed decompression of cabs in the `_Streams` pseudo table

#### 7-November-2002 - Release 2.1.4
- fixed compression of zero-size files

#### 6-November-2002 - Release 2.1.3
- fixed command line parsing

#### 6-November-2002 - Release 2.1.2
- completed `-c` (`--extract-cabs`) switch with comma-separated list of media cabs to extract

#### 4-November-2002 - Release 2.1.1
- fixed MsiFileHash calcuation
- added tooltips with column information to the default stylesheet

#### 4-November-2002 - Release 2.1.0
- added long command line options support
- added new options to xml2msi to update/generated GUID for product/package/upgrade codes, product version
- added new option to update 'Update' table with new version number

#### 3-November-2002 - Release 2.0.0
- added cabinet file decompression / compression support
- new option for xml2msi writes out update XML containing new version numbers, MD5 hashes, etc.   

#### 27-October-2002 - Release 1.4.3
- Fixed `-d` option in msi2xml.

#### 20-August-2002 - Release 1.4.2
- Fixed mysterious md5 string bug simply by recompiling...
- I am distributing only the Unicode version now. Win9X support is provided through unicow.dll.
- Reformatted source.

#### 6-August-2002 - Release 1.4.1
- Fixed bug [ #578884 ] msi2xml.cpp missing #include <limits>.
- Fixed bug [ #578888 ] Inappropriate HRESULTS thrown.

#### 14-April-2002 - Release 1.4.0
- Fixed syntax error in xml-stylesheet PI    
- Updated to MSXML 4.0
- Created VC.NET project files
- Put #import statement in conditional clause, to allow for compilation with gcc.

#### 14-January-2002 - Release 1.3.0
- Fixed bug [ #502029 ] msi2xml drops the code page value.
- Fixed bug [ #502444 ] xml2msi failed to read Japanese chars.
- Added "codepage" attribute to `<msi>` tag.
- Changed default XML encoding to US-ASCII (non-US-ASCII characters will be encoded as `&#xnnnn;` where nnnn is the character's Unicode value.
- Changed to MSXML 4.0
- Fixed bug in table row sorting.
- Made all buffers dynamic.
- Added validation info for new Windows Installer 2.0 tables.

#### 5-January-2002 - Release 1.2.1
- Fixed bug [ #492069 ] Invalid character in xml output.

#### 15-Nov-2001 - Release 1.2.0
- Huge bug fix: unreferenced binary streams were not exported by previous versions. Msi2xml now exports ALL binary streams, and places unreferenced streams in the `_Streams` table. (Note that it does not matter if streams appear in a regular table and again in the `_Streams` pseudo table, as long as the name value is of the form Table.StreamId.)
- Added `-p` command line switch to xml2msi to specify a path prefix for external references.

#### 25-Sept-2001 - Release 1.1.10     
- Added `-d` command line switch to msi2xml to disable datatypes in XML file.

#### 8-June-2001 - Release 1.1.9
- Failed missing Summary Properties in XML2MSI.

#### 28-May-2001 - Release 1.1.8
- Fixed failed conversion when Summary Property empty in XML file.

#### 14-May-2001 - Release 1.1.7
- Fixed crash in xml2msi when checksum verification fails.

#### 10-May-2001 - Release 1.1.6
- Msi2xml no longer issues an "Unknown Property" error if some of the Summary Info Properties are missing.

#### 9-May-2001 - Release 1.1.5
- Msi2xml does not change the time stamps of external binary streams if their content has not changed.
- Speed improvements for msi2xml (80% faster).
- Added cross-referencing hyperlinks to style sheet.

#### 8-May-2001 - Release 1.1.4
- Added summary information to XSL style sheet (contributed by Dan Rolander).

#### 7-May-2001 - Release 1.1.3
- Added sorting according to all key columns.

#### 4-May-2001 - Release 1.1.2
- Added option to sort rows according to the first field (enabled by default).

#### 2-May-2001 - Release 1.1.0
- Initial release of xml2msi/msi2xml 1.1

## License

The binaries and part of the source files are distributed under the terms of the MIT License.

Copyright (c) 2001-2007 Daniel Gehriger