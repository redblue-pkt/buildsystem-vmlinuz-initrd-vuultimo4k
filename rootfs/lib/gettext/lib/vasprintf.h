/* vsprintf with automatic memory allocation.
   Copyright (C) 2002 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _VASPRINTF_H
#define _VASPRINTF_H

#ifndef PARAMS
# if __STDC__ || defined __GNUC__ || defined __SUNPRO_C || defined __cplusplus || __PROTOTYPES
#  define PARAMS(Args) Args
# else
#  define PARAMS(Args) ()
# endif
#endif

#if HAVE_VASPRINTF

/* Get asprintf(), vasprintf() declarations.  */
#include <stdio.h>

#else

/* Get va_list.  */
#if __STDC__ || defined __cplusplus
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#ifndef __attribute__
/* This feature is available in gcc versions 2.5 and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 5) || __STRICT_ANSI__
#  define __attribute__(Spec) /* empty */
# endif
/* The __-protected variants of `format' and `printf' attributes
   are accepted by gcc versions 2.6.4 (effectively 2.7) and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#  define __format__ format
#  define __printf__ printf
# endif
#endif

#ifdef	__cplusplus
extern "C" {
#endif

extern int asprintf PARAMS ((char **result, const char *format, ...))
       __attribute__ ((__format__ (__printf__, 2, 3)));
extern int vasprintf PARAMS ((char **result, const char *format, va_list args))
       __attribute__ ((__format__ (__printf__, 2, 0)));

#ifdef	__cplusplus
}
#endif

#endif

#endif /* _VASPRINTF_H */