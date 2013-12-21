/* 
   The JitterBug bug tracking system
   a simple vslprintf for systems that don't have vsnprintf

   Copyright (C) Andrew Tridgell 1997

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
/*
 * Modified for OS/2 by harald.kipp@egnite.de.
 * All these additional modifications are public domain.
 *
 * $Log: vslprintf.c,v $
 * Revision 1.2  2000/04/25 19:52:11  harald
 * First OS/2 release
 *
 */

#include "jitterbug.h"

#ifndef HAVE_VSNPRINTF
static FILE *nullf;
#endif

/* slprintf() and vslprintf() are much like snprintf() and vsnprintf() except that:

   1) they _always_ null terminate
   2) they work on systems that don't have vsnprintf
   3) attempts to write more than n characters (including the null) produce a fatal error
*/

int vslprintf(char *str, int n, char *format, va_list ap)
{
	int ret;

	ret = vsnprintf(str, n, format, ap);
	if (ret > n || ret < 0) {
		str[n] = 0;
		fatal("string overflow with format [%20.20s]", format);
	}
	str[ret] = 0;
	return ret;
}


int slprintf(char *str, int n, char *format, ...)
{
	va_list ap;  
	int ret;

	va_start(ap, format);
	ret = vslprintf(str,n,format,ap);
	va_end(ap);
	return ret;
}
