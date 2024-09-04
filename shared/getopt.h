//------------------------------------------------------------------------------
//
// $Id: getopt.h 61 2002-11-04 17:33:17Z dgehriger $
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
//------------------------------------------------------------------------------
#ifndef GETOPT_H_INCLUDED
#define GETOPT_H_INCLUDED
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <tchar.h>

//------------------------------------------------------------------------------
// Structure definition for getopt_long
//
//  The meanings of the different fields are:
//
//  name
//    is the name of the long option.
//
//  has_arg
//    no_argument (or 0) if the option does not take an argument,
//    required_argument (or 1) if the option requires an argument, or
//    optional_argument (or 2) if the option takes an optional argument.
//
//  flag
//    specifies how results are returned for a long option.  If flag is
//    NULL, then getopt_long() returns val.  (For example, the calling
//    program may set val to the equivalent short option character.)
//    Otherwise, getopt_long() returns 0, and flag points to a variable
//    which is set to val if the option is found, but left unchanged if
//    the option is not found.
//
//  val
//    is the value to return, or to load into the variable pointed to by
//    flag.
//
//  The last element of the array has to be filled with zeroes.
//
//------------------------------------------------------------------------------
struct Option {
  const _TCHAR* name;
  int           has_arg;
  int*          flag;
  int           val;
};

// Symbolic constants for getopt_long
#define no_argument 0		    // the option does not take an argument
#define required_argument 1	    // the option requires an argument
#define optional_argument 2	    // the option takes an optional argument

// Global variables for getopt_long
extern int       opterr;            // if error message should be printed
extern int       optind;            // index into parent argv vector
extern int       optopt;            // character checked for validity
extern int       optreset;          // reset getopt
extern _TCHAR*   optarg;            // argument associated with option

// Function prototype for getopt_long
int getopt_long(int argc, _TCHAR * const * argv, const _TCHAR* optstring, const Option* longopts, int* longindex);

#endif // GETOPT_H_INCLUDED
