/* 
   The JitterBug bug tracking system
   utility functions

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
 * $Log: jitterbug.h,v $
 * Revision 1.2  2000/04/25 19:52:09  harald
 * First OS/2 release
 *
 */

#include "config.h"
#include "includes.h"
#include "jconfig.h"
#include "version.h"

struct message_info {
	size_t size;
	int loaded;
	int private;
	char *from;
	char *subject;
	char *date;
	char *raw;
};

#define DEBUG_COMMENT(txt) printf("<!==  Debug:        " #txt "==>\n")

#include "proto.h"

#define NS(s) ((s)?(s):"(NULL)")
