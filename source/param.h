
/*****************************************************************************
 *                                                                           *
 * sys/param.c                                                               *
 *                                                                           *
 * Freely redistributable and modifiable.  Use at your own risk.             *
 *                                                                           *
 * Copyright 1994 The Downhill Project                                       *
 *                                                                           *
 *****************************************************************************/
/*
 * Modified by harald.kipp@egnite.de.
 * All these additional modifications are public domain.
 *
 * $Log: param.h,v $
 * Revision 1.1  2000/04/25 19:52:10  harald
 * First OS/2 release
 *
 */
#ifndef MAXPATHLEN
#define MAXPATHLEN     _MAX_PATH
#endif
#define MAXNAMLEN      _MAX_FNAME
#define howmany(x,y)   (((x)+((y)-1))/(y))
#define roundup(x,y)   ((((x)+((y)-1))/(y))*(y))
