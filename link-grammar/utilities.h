/*************************************************************************/
/* Copyright (c) 2004                                                    */
/* Daniel Sleator, David Temperley, and John Lafferty                    */
/* All rights reserved                                                   */
/*                                                                       */
/* Use of the link grammar parsing system is subject to the terms of the */
/* license set forth in the LICENSE file included with this software,    */ 
/* and also available at http://www.link.cs.cmu.edu/link/license.html    */
/* This license allows free redistribution and use in source and binary  */
/* forms, with or without modification, subject to certain conditions.   */
/*                                                                       */
/*************************************************************************/
#ifndef _LINK_GRAMMAR_UTILITIES_H_
#define _LINK_GRAMMAR_UTILITIES_H_

#ifndef _WIN32
#include <langinfo.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#ifndef __CYGWIN__
#include <wchar.h>
#include <wctype.h>
#endif

#include "error.h"

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifdef __CYGWIN__
#define _WIN32 1
#endif /* __CYGWIN__ */

#ifdef _WIN32
#include <windows.h>

#ifndef _MSC_VER
   /* CYGWIN/MINGW compilers don't support the "inline" keyword. */
#define inline
#endif

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

/* CYGWIN on Windows doesn't have UTF8 support, or wide chars ... 
 * However, MS Visual C appearnetly does ... 
 */
#ifdef __CYGWIN__
#define mbtowc(w,s,n)  ({*((char *)(w)) = *(s); 1;})
#define wctomb(s,w)    ({*((char *)(s)) = ((char)(w)); 1;})
#define iswupper  isupper
#define iswalpha  isalpha
#define iswdigit  isdigit
#define iswspace  isspace
#define wchar_t   char
#define wint_t    int
#define fgetwc    fgetc
#define WEOF      EOF
#define towlower  tolower
#define towupper  toupper
#endif

/* strtok_r is missing in windows */
char * strtok_r (char *s, const char *delim, char **saveptr);

#endif /* _WIN32 */

static inline int wctomb_check(char *s, wchar_t wc)
{
	int nr = wctomb(s, wc);
	if (nr < 0) {
#ifndef _WIN32
		prt_error("Fatal Error: unknwon character set %s\n", nl_langinfo(CODESET));
#else
		prt_error("Fatal Error: unknown character set\n");
#endif
		exit(1);
	}
	return nr;
}

static inline int is_utf8_upper(const char *s)
{
	wchar_t c;
	int nbytes = mbtowc(&c, s, 4);
	if (iswupper(c)) return nbytes;
	return 0;
}

static inline int is_utf8_alpha(const char *s)
{
	wchar_t c;
	int nbytes = mbtowc(&c, s, 4);
	if (iswalpha(c)) return nbytes;
	return 0;
}

static inline int is_utf8_digit(const char *s)
{
	wchar_t c;
	int nbytes = mbtowc(&c, s, 4);
	if (iswdigit(c)) return nbytes;
	return 0;
}

static inline int is_utf8_space(const char *s)
{
	wchar_t c;
	int nbytes = mbtowc(&c, s, 4);
	if (iswspace(c)) return nbytes;
	return 0;
}

static inline const char * skip_utf8_upper(const char * s)
{
	int nb = is_utf8_upper(s);
	while (nb)
	{
		s += nb;
		nb = is_utf8_upper(s);
	}
	return s;
}

/**
 * Return true if the intial upper-case letters of the
 * two input strings match. Comparison stops when
 * both srings descend to lowercase.
 */
static inline int utf8_upper_match(const char * s, const char * t)
{
	wchar_t ws, wt;
	int ns, nt;

	ns = mbtowc(&ws, s, 4);
	nt = mbtowc(&wt, t, 4);
	while (iswupper(ws) || iswupper(wt))
	{
		if (ws != wt) return FALSE;
		s += ns;
		t += nt;
		ns = mbtowc(&ws, s, 4);
		nt = mbtowc(&wt, t, 4);
	}
	return TRUE;
}

void downcase_utf8_str(char *to, const char * from, size_t usize);
void upcase_utf8_str(char *to, const char * from, size_t usize);

void safe_strcpy(char *u, const char * v, size_t usize);
void safe_strcat(char *u, const char *v, size_t usize);
char *safe_strdup(const char *u);

void init_randtable(void);
void left_print_string(FILE* fp, const char *, const char *);

/* routines for allocating basic objects */
void init_memusage(void);
void * xalloc(size_t);
void * xrealloc(void *, size_t oldsize, size_t newsize);
void xfree(void *, size_t);
void * exalloc(size_t);
void exfree(void *, size_t);
size_t get_space_in_use(void);
size_t get_max_space_used(void);


char * get_default_locale(void);
char * join_path(const char * prefix, const char * suffix);

FILE *dictopen(const char *filename, const char *how);
void set_data_dir(const char * path);

/**
 * Returns the smallest power of two that is at least i and at least 1
 */
static inline int next_power_of_two_up(int i)
{
   int j=1;
   while(j<i) j = j<<1;
   return j;
}

#endif
