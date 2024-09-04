;echo off

set REF=msi2xml\Release_Unicode\msi2xml.exe
for /f %%i in ('getversion\Release_Unicode\getversion.exe %REF%') do (set VER=%%i)

xml2msi\Release_Unicode\xml2msi --package-code --product-code --product-version=%REF% --upgrade-version --update-xml --output=msi2xml-%VER%.msi msi2xml.xml

pkzip25 -add -path=specify msi2xml-%VER%-src.zip  @files.lst
pkzip25 -add msi2xml-%VER%-bin.zip msi2xml\Release_Unicode\msi2xml.exe xml2msi\Release_Unicode\xml2msi.exe cabs\unicows.dll

call srcsrv\svnindex.cmd .
pkzip25 -add symbols.zip msi2xml\Release_Unicode\msi2xml.exe xml2msi\Release_Unicode\xml2msi.exe cabs\unicows.dll msi2xml\Release_Unicode\msi2xml.pdb xml2msi\Release_Unicode\xml2msi.pdb


