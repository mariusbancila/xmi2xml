//------------------------------------------------------------------
//
// $Id: coldefs.h 72 2002-11-05 09:28:23Z dgehriger $
//
// Copyright (c) 2001-2002 Daniel Gehriger <gehriger at linkcad dot com>
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
#ifndef XML2MSI_COLDEFS_INCLUDED
#define XML2MSI_COLDEFS_INCLUDED
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

struct ColDef 
{ 
  LPCTSTR szTable;
  LPCTSTR szDef;
};

static const ColDef colDefs[] = 
{
    { _T("ActionText"),             _T("Ys72;NL64;NL128") },
    { _T("AdminExecuteSequence"),   _T("Ys72;NS255;NI2") },
    { _T("AdminUISequence"),        _T("Ys72;NS255;NI2") },
    { _T("AdvtExecuteSequence"),    _T("Ys72;NS255;NI2") },
    { _T("AdvtUISequence"),         _T("Ys72;NS255;NI2") },
    { _T("AppId"),                  _T("Ys38;NS255;NS255;NS255;NS255;NI2;NI2") },
    { _T("AppSearch"),              _T("Ys72;Ys72") },
    { _T("MsiAssembly"),            _T("Ys72;Ns32;NS72;NS72;Ni2") },
    { _T("MsiAssemblyName"),        _T("Ys72;Ns255;NS255") },
    { _T("BBControl"),              _T("Ys50;Ys50;Ns50;Ni2;Ni2;Ni2;Ni2;NI4;NL50") },
    { _T("Billboard"),              _T("Ys50;Ns32;NS50;NI2") },
    { _T("Binary"),                 _T("Ys72;Nv0") },
    { _T("BindImage"),              _T("Ys72;NS255") },
    { _T("CCPSearch"),              _T("Ys72") },
    { _T("CheckBox"),               _T("Ys72;NS64") },
    { _T("Class"),                  _T("Ys38;Ys32;Ys72;NS255;NL255;NS38;NS255;NS72;NI2;NS32;NS255;Ns32;NI2") },
    { _T("ComboBox"),               _T("Ys72;Yi2;Ns64;NL64") },
    { _T("CompLocator"),            _T("Ys72;Ns38;NI2") },
    { _T("Complus"),                _T("Ys72;NI2") },
    { _T("Component"),              _T("Ys72;NS38;Ns72;Ni2;NS255;NS72") },
    { _T("Condition"),              _T("Ys32;Yi2;NS255") },
    { _T("Control"),                _T("Ys72;Ys50;Ns20;Ni2;Ni2;Ni2;Ni2;NI4;NS50;NL0;NS50;NL50") },
    { _T("ControlCondition"),       _T("Ys72;Ys50;Ys50;Ys255") },
    { _T("ControlEvent"),           _T("Ys72;Ys50;Ys50;Ys255;YS255;NI2") },
    { _T("CreateFolder"),           _T("Ys72;Ys72") },
    { _T("CustomAction"),           _T("Ys72;Ni2;NS64;NS255") },
    { _T("Dialog"),                 _T("Ys72;Ni2;Ni2;Ni2;Ni2;NI4;NL128;Ns50;NS50;NS50") },
    { _T("Directory"),              _T("Ys72;NS72;Nl255") },
    { _T("DrLocator"),              _T("Ys72;YS72;YS255;NI2") },
    { _T("DuplicateFile"),          _T("Ys72;Ns72;Ns72;NL255;NS32") },
    { _T("Environment"),            _T("Ys72;Nl255;NL255;Ns72") },
    { _T("Error"),                  _T("Yi2;NL255") },
    { _T("EventMapping"),           _T("Ys72;Ys50;Ys50;Ns50") },
    { _T("Extension"),              _T("Ys255;Ys72;NS255;NS64;Ns32") },
    { _T("Feature"),                _T("Ys32;NS32;NL64;NL255;NI2;Ni2;NS72;Ni2") },
    { _T("FeatureComponents"),      _T("Ys32;Ys72") },
    { _T("File"),                   _T("Ys72;Ns72;Nl255;Ni4;NS72;NS20;NI2;Ni2") },
    { _T("FileSFPCatalog"),         _T("Ys72;YNl255") },
    { _T("Font"),                   _T("Ys72;NS128") },
    { _T("Icon"),                   _T("Ys72;Nv0") },
    { _T("IniFile"),                _T("Ys72;Nl255;NS72;Nl96;Nl128;Nl255;Ni2;Ns72") },
    { _T("IniLocator"),             _T("Ys72;Ns255;Ns96;Ns128;NI2;NI2") },
    { _T("InstallExecuteSequence"), _T("Ys72;NS255;NI2") },
    { _T("InstallUISequence"),      _T("Ys72;NS255;NI2") },
    { _T("IsolatedComponent"),      _T("Ys72;Ys72") },
    { _T("LaunchCondition"),        _T("Ys255;Nl255") },
    { _T("ListBox"),                _T("Ys72;Yi2;Ns64;NL64") },
    { _T("ListView"),               _T("Ys72;Yi2;Ns64;NL64;NS72") },
    { _T("LockPermissions"),        _T("Ys72;Ys32;YS255;Ys255;NI4") },
    { _T("MIME"),                   _T("Ys64;Ns255;NS38") },
    { _T("Media"),                  _T("Yi2;Ni2;NL64;NS255;NS32;NS32") },
    { _T("ModuleComponents"),       _T("Ys72;Ys72;Ni2") },
    { _T("ModuleSignature"),        _T("Ys72;Yi2;Ns72") },
    { _T("ModuleDependency"),       _T("Ys72;Yi2;Ys72;Yi2;NS72") },
    { _T("ModuleExclusion"),        _T("Ys72;Yi2;Ys72;Yi2;NS72;NS72") },
    { _T("ModuleAdminUISequence"),  _T("Ys72;NI2;NS72;NI2;NS255") },
    { _T("ModuleAdminExecuteSequence"),_T("Ys72;NI2;NS72;NI2;NS255") },
    { _T("ModuleAdvtUISequence"),   _T("Ys72;NI2;NS72;NI2;NS255") },
    { _T("ModuleAdvtExecuteSequence"),_T("Ys72;NI2;NS72;NI2;NS255") },
    { _T("ModuleIgnoreTable"),      _T("Ys72") },
    { _T("ModuleInstallUISequence"),_T("Ys72;NI2;NS72;NI2;NS255") },
    { _T("ModuleInstallExecuteSequence"),_T("Ys72;NI2;NS72;NI2;NS255") },
    { _T("MoveFile"),               _T("Ys72;Ns72;NL255;NL255;NS72;Ns72;Ni2") },
    { _T("MsiDigitalCertificate"),  _T("Ys72;Nv0") },
    { _T("MsiDigitalSignature"),    _T("Ys72;Ys255;Ns72;NV0") },
    { _T("MsiFileHash"),            _T("Ys72;Ni2;Ni4;Ni4;Ni4") },
    { _T("MsiPatchHeaders"),        _T("Ys72;Nv0") },
    { _T("ODBCAttribute"),          _T("Ys72;Ys40;NL255") },
    { _T("ODBCDataSource"),         _T("Ys72;Ns72;Ns255;Ns255;Ni2") },
    { _T("ODBCDriver"),             _T("Ys72;Ns72;Ns255;Ns72;NS72") },
    { _T("ODBCSourceAttribute"),    _T("Ys72;Ys32;NL255") },
    { _T("ODBCTranslator"),         _T("Ys72;Ns72;Ns255;Ns72;NS72") },
    { _T("Patch"),                  _T("Ys72;Yi2;Ni4;Ni2;Nv0") },
    { _T("PatchPackage"),           _T("Ys38;Ni2") },
    { _T("ProgId"),                 _T("Ys255;NS255;NS38;NL255;NS72;NI2") },
    { _T("Property"),               _T("Ys72;Nl0") },
    { _T("PublishComponent"),       _T("Ys38;Ys255;Ys72;NL255;Ns32") },
    { _T("RadioButton"),            _T("Ys72;Yi2;Ns64;Ni2;Ni2;Ni2;Ni2;NL64;NL50") },
    { _T("RegLocator"),             _T("Ys72;Ni2;Ns255;NS255;NI2") },
    { _T("Registry"),               _T("Ys72;Ni2;Nl255;NL255;NL0;Ns72") },
    { _T("RemoveFile"),             _T("Ys72;Ns72;NL255;Ns72;Ni2") },
    { _T("RemoveIniFile"),          _T("Ys72;Nl255;NS72;Nl96;Nl128;NL255;Ni2;Ns72") },
    { _T("RemoveRegistry"),         _T("Ys72;Ni2;Nl255;NL255;Ns72") },
    { _T("ReserveCost"),            _T("Ys72;Ns72;NS72;Ni4;Ni4") },
    { _T("SelfReg"),                _T("Ys72;NI2") },
    { _T("ServiceControl"),         _T("Ys72;Nl255;Ni2;NL255;NI2;Ns72") },
    { _T("ServiceInstall"),         _T("Ys72;Ns255;NL255;Ni4;Ni4;Ni4;NS255;NS255;NS255;NS255;NS255;Ns72;NL255") },
    { _T("SFPCatalog"),             _T("Ysl255;Nv0;NL0") },
    { _T("Shortcut"),               _T("Ys72;Ns72;Nl128;Ns72;Ns72;NS255;NL255;NI2;NS72;NI2;NI2;NS72") },
    { _T("Signature"),              _T("Ys72;Ns255;NS20;NS20;NI4;NI4;NI4;NI4;NS255") },
    { _T("TextStyle"),              _T("Ys72;Ns32;Ni2;NI4;NI2") },
    { _T("TypeLib"),                _T("Ys38;Yi2;Ys72;NI2;NL128;NS72;Ns32;NI4") },
    { _T("UIText"),                 _T("Ys72;NL255") },
    { _T("Upgrade"),                _T("Ys38;YS20;YS20;YS255;Yi4;NS255;Ns72") },
    { _T("Verb"),                   _T("Ys255;Ys32;NI2;NL255;NL255") },
    { _T("_Streams"),               _T("Ys62;NV0") },
    { _T("_Validation"),            _T("Ys32;Ys32;Ns4;NI4;NI4;NS255;NI2;NS32;NS255;NS255") }
};

#endif // XML2MSI_COLDEFS_INCLUDED

