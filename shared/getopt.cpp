//------------------------------------------------------------------------------
//
// $Id: getopt.cpp 94 2007-09-26 13:51:37Z dgehriger $
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
#include "getopt.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

int       opterr = 1;                   // if error message should be printed
int       optind = 1;                   // index into parent argv vector
int       optopt = _T('?');             // character checked for validity
int       optreset;                     // reset getopt
_TCHAR*   optarg;                       // argument associated with option

#define IGNORE_FIRST        (*options == _T('-') || *options == _T('+'))
#define PRINT_ERROR         ((opterr) && ((*options != _T(':')) || (IGNORE_FIRST && options[1] != _T(':'))))
#define IS_POSIXLY_CORRECT  (_tgetenv(_T("POSIXLY_CORRECT")) != NULL)
#define PERMUTE             (!IS_POSIXLY_CORRECT && !IGNORE_FIRST)

// XXX: GNU ignores PC if *options == '-'
#define IN_ORDER  (!IS_POSIXLY_CORRECT && *options == _T('-'))

// return values
#define BADCH   (int)_T('?')
#define BADARG  (int)_T(':')
#define INORDER (int)1

#define EMSG    _T("")

static int          getopt_internal(int, _TCHAR * const *, const _TCHAR *);
static int          gcd(int, int);
static void         permute_args(int, int, int, _TCHAR * const *);

static _TCHAR*      place = EMSG;       // option letter processing

// XXX: set optreset to 1 rather than these two
static int          nonopt_start = -1;  // first non option argument (for permute)
static int          nonopt_end = -1;    // first option after non options (for permute)

// Error messages
static const _TCHAR recargchar[]    = _T("option requires an argument -- %c");
static const _TCHAR recargstring[]  = _T("option requires an argument -- %s");
static const _TCHAR ambig[]         = _T("ambiguous option -- %.*s");
static const _TCHAR noarg[]         = _T("option doesn't take an argument -- %.*s");
static const _TCHAR illoptchar[]    = _T("illegal option -- %c");
static const _TCHAR illoptstring[]  = _T("illegal option -- %s");

extern _TCHAR*      __progname;

//------------------------------------------------------------------------------
// Compute the greatest common divisor of a and b.
//------------------------------------------------------------------------------
static int gcd (int a, int b)
{
  int c;

  c = a % b;
  while (c != 0) {
    a = b;
    b = c;
    c = a % b;
  }
  return b;
}

//------------------------------------------------------------------------------
// Exchange the block from nonopt_start to nonopt_end with the block from
// nonopt_end to opt_end (keeping the same order of arguments in each block).
//------------------------------------------------------------------------------
static void permute_args(int nonopt_start, int nonopt_end, int opt_end, _TCHAR* const * nargv)
{
  int       cstart, cyclelen, i, j, ncycle, nnonopts, nopts, pos;
  _TCHAR*   swap;

  // compute lengths of blocks and number and size of cycles
  nnonopts = nonopt_end - nonopt_start;
  nopts = opt_end - nonopt_end;
  ncycle = gcd(nnonopts, nopts);
  cyclelen = (opt_end - nonopt_start) / ncycle;

  for (i = 0; i < ncycle; i++) {
    cstart = nonopt_end + i;
    pos = cstart;
    for (j = 0; j < cyclelen; j++) {
      if (pos >= nonopt_end)
        pos -= nnonopts;
      else
        pos += nopts;
      swap = nargv[pos];

      // LINTED const cast
      ((_TCHAR**)nargv)[pos] = nargv[cstart];

      // LINTED const cast
      ((_TCHAR**)nargv)[cstart] = swap;
    }
  }
}

//------------------------------------------------------------------------------
// getopt_internal
// Parse argc/argv argument vector. Called by user level routines. 
// Returns: -2 if -- is found (can be long option or end of options marker).
//------------------------------------------------------------------------------
static int getopt_internal(int nargc, _TCHAR * const * nargv, const _TCHAR* options)
{
  const _TCHAR*   oli;              // option letter list index
  int       optchar;

  optarg = NULL;

  // XXX Some programs (like rsyncd) expect to be able to XXX re-initialize
  // optind to 0 and have getopt_long(3) XXX properly function again. Work
  // around this braindamage.
  if (optind == 0) optind = 1;
  if (optreset) nonopt_start = nonopt_end = -1;
start:
  if (optreset || !*place) {  // update scanning pointer
    optreset = 0;
    if (optind >= nargc) {    // end of argument vector
      place = EMSG;
      if (nonopt_end != -1) {
        // do permutation, if we have to
        permute_args(nonopt_start, nonopt_end, optind, nargv);
        optind -= nonopt_end - nonopt_start;
      }
      else if (nonopt_start != -1) {
        // If we skipped non-options, set optind to the first of them.
        optind = nonopt_start;
      }
      nonopt_start = nonopt_end = -1;
      return -1;
    }
    if (*(place = nargv[optind]) != _T('-')) {  // found non-option
      place = EMSG;
      if (IN_ORDER) {
        // GNU extension: return non-option as argument to option 1
        optarg = nargv[optind++];
        return INORDER;
      }
      if (!PERMUTE) {
        // if no permutation wanted, stop parsing at first non-option
        return -1;
      }

      // do permutation
      if (nonopt_start == -1)
        nonopt_start = optind;
      else if (nonopt_end != -1) {
        permute_args(nonopt_start, nonopt_end, optind, nargv);
        nonopt_start = optind - (nonopt_end - nonopt_start);
        nonopt_end = -1;
      }
      optind++;

      // process next argument
      goto start;
    }
    if (nonopt_start != -1 && nonopt_end == -1) nonopt_end = optind;
    if (place[1] && *++place == _T('-')) {      // found "--"
      place++;
      return -2;
    }
  }
  if ((optchar = (int) *place++) == (int)_T(':')
    || (oli = _tcschr(options + (IGNORE_FIRST ? 1 : 0), optchar)) == NULL) {
    // option letter unknown or ':'
    if (!*place) ++optind;
    if (PRINT_ERROR) _tprintf(illoptchar, optchar);
    optopt = optchar;
    return BADCH;
  }
  if (optchar == _T('W') && oli[1] == _T(';')) {    // -W long-option
    // XXX: what if no long options provided (called by getopt)?
    if (*place) return -2;

    if (++optind >= nargc) {                // no arg
      place = EMSG;
      if (PRINT_ERROR) _tprintf(recargchar, optchar);
      optopt = optchar;

      // XXX: GNU returns '?' if options[0] != ':'
      return BADARG;
    }
    else                      // white space
      place = nargv[optind];

    // Handle -W arg the same as --arg (which causes getopt to stop parsing).
    return -2;
  }
  if (*++oli != _T(':')) {        // doesn't take argument
    if (!*place) ++optind;
  }
  else {                      // takes (optional) argument
    optarg = NULL;
    if (*place)               // no white space
      optarg = place;

    // XXX: disable test for :: if PC? (GNU doesn't)
    else if (oli[1] != _T(':')) { // arg not optional
      if (++optind >= nargc) {  // no arg
        place = EMSG;
        if (PRINT_ERROR) _tprintf(recargchar, optchar);
        optopt = optchar;

        // XXX: GNU returns _T('?') if options[0] != ':'
        return BADARG;
      }
      else
        optarg = nargv[optind];
    }
    place = EMSG;
    ++optind;
  }

  // dump back option letter
  return optchar;
}

//------------------------------------------------------------------------------
// getopt -- Parse argc/argv argument vector. [eventually this will replace
// the real getopt]
//------------------------------------------------------------------------------
int getopt (int nargc, _TCHAR * const * nargv, const _TCHAR* options)
{
  int retval;

  if ((retval = getopt_internal(nargc, nargv, options)) == -2) {
    ++optind;

    // We found an option (--), so if we skipped non-options, we have to
    // permute.
    if (nonopt_end != -1) {
      permute_args(nonopt_start, nonopt_end, optind, nargv);
      optind -= nonopt_end - nonopt_start;
    }
    nonopt_start = nonopt_end = -1;
    retval = -1;
  }
  return retval;
}

// -----------------------------------------------------------------------------
//  getopt_long -- Parse argc/argv argument vector.
// -----------------------------------------------------------------------------
int getopt_long(int             nargc,
                _TCHAR* const   nargv[],
                const _TCHAR*   options,
                const Option*   long_options,
                int*            idx)
{
  int retval;

  // idx may be NULL
  if ((retval = getopt_internal(nargc, nargv, options)) == -2) {
    _TCHAR*   current_argv, *has_equal;
    size_t    current_argv_len;
    int       i, match;

    current_argv = place;
    match = -1;

    optind++;
    place = EMSG;

    if (*current_argv == _T('\0')) {  // found "--"
      // We found an option (--), so if we skipped non-options, we have to
      // permute.
      if (nonopt_end != -1) {
        permute_args(nonopt_start, nonopt_end, optind, nargv);
        optind -= nonopt_end - nonopt_start;
      }

      nonopt_start = nonopt_end = -1;
      return -1;
    }

    if ((has_equal = _tcschr(current_argv, _T('='))) != NULL) {
      // argument found (--option=arg)
      current_argv_len = has_equal - current_argv;
      has_equal++;
    }
    else
      current_argv_len = _tcslen(current_argv);

    for (i = 0; long_options[i].name; i++) {
      // find matching long option
      if (_tcsncmp(current_argv, long_options[i].name, current_argv_len))
        continue;

      if (_tcslen(long_options[i].name) == (unsigned)current_argv_len) {
        // exact match
        match = i;
        break;
      }

      if (match == -1)            // partial match
        match = i;
      else {
        // ambiguous abbreviation
        if (PRINT_ERROR) _tprintf(ambig, (int)current_argv_len, current_argv);
        optopt = 0;
        return BADCH;
      }
    }

    if (match != -1) {            // option found
      if (long_options[match].has_arg == no_argument && has_equal) {
        if (PRINT_ERROR) _tprintf(noarg, (int)current_argv_len, current_argv);

        // XXX: GNU sets optopt to val regardless of flag
        if (long_options[match].flag == NULL)
          optopt = long_options[match].val;
        else
          optopt = 0;

        // XXX: GNU returns '?' if options[0] != ':'
        return BADARG;
      }

      if (long_options[match].has_arg == required_argument
        || long_options[match].has_arg == optional_argument) {
        if (has_equal)
          optarg = has_equal;
        else if (long_options[match].has_arg == required_argument) {
          // optional argument doesn't use next nargv
          optarg = nargv[optind++];
        }
      }

      if ((long_options[match].has_arg == required_argument)
        && (optarg == NULL)) {
        // Missing argument; leading ':' indicates no error should be
        // generated
        if (PRINT_ERROR) _tprintf(recargstring, current_argv);

        // XXX: GNU sets optopt to val regardless of flag
        if (long_options[match].flag == NULL)
          optopt = long_options[match].val;
        else
          optopt = 0;

        // XXX: GNU returns '?' if options[0] != ':'
        --optind;
        return BADARG;
      }
    }
    else {  // unknown option
      if (PRINT_ERROR) _tprintf(illoptstring, current_argv);
      optopt = 0;
      return BADCH;
    }

    if (long_options[match].flag) {
      *long_options[match].flag = long_options[match].val;
      retval = 0;
    }
    else
      retval = long_options[match].val;
    if (idx) *idx = match;
  }

  return retval;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
