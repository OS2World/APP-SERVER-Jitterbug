/* 
   The JitterBug bug tracking system
   list handling utility functions

   Copyright (C) Dan Shearer 1997
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

   This module handles simple lists of strings terminated by a NULL string

*/
/*
 * Modified for OS/2 by harald.kipp@egnite.de.
 * All these additional modifications are public domain.
 *
 * $Log: list.c,v $
 * Revision 1.2  2000/04/25 19:52:09  harald
 * First OS/2 release
 *
 */

#include "jitterbug.h"

/* These constants are a bit silly because add_item() only knows about 
MAX_LIST */
#define MAX_DIR 10000
#define MAX_LINES 10000
#define MAX_LIST 10000

/* load a directory into a list of strings. Only filenames
   which match the passed function are loaded */
char **load_dir_list(char *name, int (*fn)(char *name))
{
	char **ret;
	int i=0;
	DIR *dir;
	struct dirent *d;
	ret = (char **)malloc(MAX_DIR*sizeof(ret[0]));
	if (!ret) return NULL;

	dir = opendir(name);
	if (!dir) {
		free(ret);
		return NULL;
	}

	while ((d = readdir(dir))) {
		char *dname = d_name(d);
		if (dname[0] == '.')
			continue;
		if (fn && !fn(dname)) continue;
		ret[i] = strdup(dname);
		if (!ret[i]) {
			fatal("strdup failed\n");
			return NULL;
		}
		i++;
		if (i == MAX_DIR-1)
			break;
	}
	ret[i] = NULL;

	closedir(dir);

	if (i > 1)
		qsort(ret, i, sizeof(ret[i]), namecmp);

	return ret;
}

/* load a file into a list of strings. Only strings
   which match the passed function are loaded */
char **load_file_list(char *name,int (*fn)(char *name))
{
	char **ret;
	int i=0;
	FILE *f;
	char buf[100];

	ret = (char **)malloc(MAX_LINES*sizeof(ret[0]));
	if (!ret) return NULL; /*should have proper fatal() for util.c*/

	f = fopen(name,"r");
	if (!f) return NULL; /*ditto fatal*/

	while (fgets(buf, sizeof(buf)-1, f)) {
		if (buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = 0;
		if (!*buf || !fn(buf)) continue;
		ret[i] = strdup(buf);
		if (!ret[i]) {
		  fatal("strdup failed\n");
		  return NULL;
		}
		i++;
	}
	fclose(f);

	ret[i] = NULL;

	if (i > 1)
		qsort(ret, i, sizeof(ret[i]), namecmp);

	return ret;
}

/* free a list load_dir_list, load_file_list or new_list has created. Relies 
on last member being NULL so use qsort elsewhere to avoid holes */
void free_list(char **list)
{
	int i;
	if (!list) return;
	for (i=0;list[i];i++)
		free(list[i]);
	free(list);
}

/* write a file with supplied mode from a list of strings. Only strings
   which match the passed function are written. Relies on last item==NULL 
   Deletes file if no entries in the list and mode is "w".
*/
int write_list(char *fname,char *mode,char **list,int (*fn)(char *name))
{
	int i;
	FILE *f;

	if (!list || !list[0]) return 0;

	for (i=0;list[i];i++);

	if (!mode || !*mode || !(f=fopen(fname,mode))) return 1;

	if (strcmp(mode,"w")==0 && i==0) {
	        unlink(fname);
	} else {
	        for (i=0;list[i];i++) {
		        if (!fn(list[i])) continue;
			fprintf(f,"%s\n",list[i]);
		}
        }
	fclose(f);

	return 0;
}

void debug_print_list(char **list)
{
        int i;

        if (!list || !list[0]) {
	        printf("debug_print_list: empty list\n");
	        return;
	}

        for (i=0;list[i];i++);
	printf("debug_print_list: there are %d items\n",i);

	for (i=0;list[i];i++) {
                printf("%s<br>\n",list[i]);
	}
}


/* free first line containing item from null-terminated list of strings */
int delete_list_item(char **list,char *item)
{
        int i,j;
	
	if (!item || !*item) return 1;
	
	for (i=0; list[i]; i++) {
		if (strcmp(list[i],item)==0) {
			free(list[i]);
			for (j=i; list[j]; j++)
				list[j] = list[j+1];
			return 0;
		}
	}

	return 1;
}

/* create a new list */
char **new_list(void)
{
        char **ret;

        ret = (char **)malloc(MAX_LIST*sizeof(ret[0]));
        if (!ret) return NULL;
        ret[0] = NULL;
	return ret;
}

/* append an item to a null-terminated list of strings */
int add_list_item(char **list,char *item)
{
        int i;

	if (!item || !*item) return 1;

	if (!list) return 1; /*caller's responsibility to make **list's */

	for (i=0; list[i]; i++);
	if (i == MAX_LIST) return 1;

	if (!(list[i]=strdup(item))) {
	        fatal("strdup failed\n");
		return 1;
	}

	if (i > 1)
		qsort(list, i, sizeof(list[i]), namecmp);
	list[i+1] = NULL;

	return 0;
}
