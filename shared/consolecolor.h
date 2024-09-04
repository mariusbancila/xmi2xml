//------------------------------------------------------------------------------
// $Id: consolecolor.h 97 2007-09-26 17:49:04Z dgehriger $
//
// Author: Jaded Hobo <http://www.codeproject.com/cpp/AddColorConsole.asp>
// Modified for TCHAR: 2007 Daniel Gehriger <gehriger at linkcad dot com>
//------------------------------------------------------------------------------
#ifndef INCLUDED_300949260716_H
#define INCLUDED_300949260716_H

#ifdef _MSC_VER
#pragma once
#endif

#include <iostream>
#include <iomanip>
#include <windows.h>
#include "tstring.h"

namespace color {

static const WORD bgMask( BACKGROUND_BLUE      | 
    BACKGROUND_GREEN     | 
    BACKGROUND_RED       | 
    BACKGROUND_INTENSITY   );
static const WORD fgMask( FOREGROUND_BLUE      | 
    FOREGROUND_GREEN     | 
    FOREGROUND_RED       | 
    FOREGROUND_INTENSITY   );

static const WORD fgBlack    ( 0 ); 
static const WORD fgLoRed    ( FOREGROUND_RED   ); 
static const WORD fgLoGreen  ( FOREGROUND_GREEN ); 
static const WORD fgLoBlue   ( FOREGROUND_BLUE  ); 
static const WORD fgLoCyan   ( fgLoGreen   | fgLoBlue ); 
static const WORD fgLoMagenta( fgLoRed     | fgLoBlue ); 
static const WORD fgLoYellow ( fgLoRed     | fgLoGreen ); 
static const WORD fgLoWhite  ( fgLoRed     | fgLoGreen | fgLoBlue ); 
static const WORD fgGray     ( fgBlack     | FOREGROUND_INTENSITY ); 
static const WORD fgHiWhite  ( fgLoWhite   | FOREGROUND_INTENSITY ); 
static const WORD fgHiBlue   ( fgLoBlue    | FOREGROUND_INTENSITY ); 
static const WORD fgHiGreen  ( fgLoGreen   | FOREGROUND_INTENSITY ); 
static const WORD fgHiRed    ( fgLoRed     | FOREGROUND_INTENSITY ); 
static const WORD fgHiCyan   ( fgLoCyan    | FOREGROUND_INTENSITY ); 
static const WORD fgHiMagenta( fgLoMagenta | FOREGROUND_INTENSITY ); 
static const WORD fgHiYellow ( fgLoYellow  | FOREGROUND_INTENSITY );
static const WORD bgBlack    ( 0 ); 
static const WORD bgLoRed    ( BACKGROUND_RED   ); 
static const WORD bgLoGreen  ( BACKGROUND_GREEN ); 
static const WORD bgLoBlue   ( BACKGROUND_BLUE  ); 
static const WORD bgLoCyan   ( bgLoGreen   | bgLoBlue ); 
static const WORD bgLoMagenta( bgLoRed     | bgLoBlue ); 
static const WORD bgLoYellow ( bgLoRed     | bgLoGreen ); 
static const WORD bgLoWhite  ( bgLoRed     | bgLoGreen | bgLoBlue ); 
static const WORD bgGray     ( bgBlack     | BACKGROUND_INTENSITY ); 
static const WORD bgHiWhite  ( bgLoWhite   | BACKGROUND_INTENSITY ); 
static const WORD bgHiBlue   ( bgLoBlue    | BACKGROUND_INTENSITY ); 
static const WORD bgHiGreen  ( bgLoGreen   | BACKGROUND_INTENSITY ); 
static const WORD bgHiRed    ( bgLoRed     | BACKGROUND_INTENSITY ); 
static const WORD bgHiCyan   ( bgLoCyan    | BACKGROUND_INTENSITY ); 
static const WORD bgHiMagenta( bgLoMagenta | BACKGROUND_INTENSITY ); 
static const WORD bgHiYellow ( bgLoYellow  | BACKGROUND_INTENSITY );

static class con_dev
{
private:
    HANDLE                      hCon;
    DWORD                       cCharsWritten; 
    CONSOLE_SCREEN_BUFFER_INFO  csbi; 
    DWORD                       dwConSize;
    WORD                        wDefault;

public:
    con_dev() 
    { 
        hCon = GetStdHandle( STD_OUTPUT_HANDLE );
        GetInfo();
        wDefault = csbi.wAttributes;
    }
private:
    void GetInfo()
    {
        GetConsoleScreenBufferInfo( hCon, &csbi );
        dwConSize = csbi.dwSize.X * csbi.dwSize.Y; 
    }
public:
    void Clear()
    {
        COORD coordScreen = { 0, 0 };

        GetInfo(); 
        FillConsoleOutputCharacter( hCon, TEXT(' '),
            dwConSize, 
            coordScreen,
            &cCharsWritten ); 
        GetInfo(); 
        FillConsoleOutputAttribute( hCon,
            csbi.wAttributes,
            dwConSize,
            coordScreen,
            &cCharsWritten ); 
        SetConsoleCursorPosition( hCon, coordScreen ); 
    }

    void SetColor( WORD wRGBI, WORD Mask )
    {
        GetInfo();
        csbi.wAttributes &= Mask; 
        csbi.wAttributes |= wRGBI; 
        SetConsoleTextAttribute( hCon, csbi.wAttributes );
    }

    void Reset()
    {
        GetInfo();
        SetConsoleTextAttribute( hCon, wDefault );
    }

} console;

inline tostream& clear( tostream& os )
{
    os.flush();
    console.Clear();
    return os;
};

inline tostream& base( tostream& os )
{
    os.flush();
    console.Reset();

    return os;
};

inline tostream& red( tostream& os )
{
    os.flush();
    console.SetColor( fgHiRed, bgMask );

    return os;
}

inline tostream& green( tostream& os )
{
    os.flush();
    console.SetColor( fgHiGreen, bgMask );

    return os;
}

inline tostream& blue( tostream& os )
{
    os.flush();
    console.SetColor( fgHiBlue, bgMask );

    return os;
}

inline tostream& white( tostream& os )
{
    os.flush();
    console.SetColor( fgHiWhite, bgMask );

    return os;
}

inline tostream& cyan( tostream& os )
{
    os.flush();
    console.SetColor( fgHiCyan, bgMask );

    return os;
}

inline tostream& magenta( tostream& os )
{
    os.flush();
    console.SetColor( fgHiMagenta, bgMask );

    return os;
}

inline tostream& yellow( tostream& os )
{
    os.flush();
    console.SetColor( fgHiYellow, bgMask );

    return os;
}

inline tostream& black( tostream& os )
{
    os.flush();
    console.SetColor( fgBlack, bgMask );

    return os;
}

inline tostream& gray( tostream& os )
{
    os.flush();
    console.SetColor( fgGray, bgMask );

    return os;
}

inline tostream& bg_red( tostream& os )
{
    os.flush();
    console.SetColor( bgHiRed, fgMask );

    return os;
}

inline tostream& bg_green( tostream& os )
{
    os.flush();
    console.SetColor( bgHiGreen, fgMask );

    return os;
}

inline tostream& bg_blue( tostream& os )
{
    os.flush();
    console.SetColor( bgHiBlue, fgMask );

    return os;
}

inline tostream& bg_white( tostream& os )
{
    os.flush();
    console.SetColor( bgHiWhite, fgMask );

    return os;
}

inline tostream& bg_cyan( tostream& os )
{
    os.flush();
    console.SetColor( bgHiCyan, fgMask );

    return os;
}

inline tostream& bg_magenta( tostream& os )
{
    os.flush();
    console.SetColor( bgHiMagenta, fgMask );

    return os;
}

inline tostream& bg_yellow( tostream& os )
{
    os.flush();
    console.SetColor( bgHiYellow, fgMask );

    return os;
}

inline tostream& bg_black( tostream& os )
{
    os.flush();
    console.SetColor( bgBlack, fgMask );

    return os;
}

inline tostream& bg_gray( tostream& os )
{
    os.flush();
    console.SetColor( bgGray, fgMask );

    return os;
}

} // namespace

#endif // INCLUDED_300949260716_H
