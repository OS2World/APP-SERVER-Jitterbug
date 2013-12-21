/* 
   The JitterBug bug tracking system
   parameter loading functions

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
 * $Log: loadparm.c,v $
 * Revision 1.2  2000/04/25 19:52:09  harald
 * First OS/2 release
 *
 */

#include "jitterbug.h"

typedef enum
{
	P_BOOL, P_INTEGER,P_STRING
} parm_type;

struct parm_struct
{
	char *label;
	parm_type type;
	void *ptr;
	unsigned flags;
	char *def;
};

#define FLAG_MANDATORY (1<<0)
#define FLAG_LOADED (1<<1)

static char *mailer;
static char *from_address;
static char *smtp_address;
static char *title;
static char *base_directory;
static char *base_url;
static char *chroot_directory;
static char *auth_user;
static char *incoming;
static char *background;
static char *search_program;
static char *forward_all;
static char *forward_public;
static char *gzip_path;
static char *reply_strings;
static char *autopatch;
static char *decoder;
static char *pr_identifier;
static int guest_gid;
static int guest_uid;
static int gid;
static int uid;
static int display_binary;
static int guest_download;
static int group_authentication;
static int gzip_download;

static struct parm_struct params[] = {
 {"incoming", P_STRING, (void *)&incoming, 0, "incoming"},
 {"mailer", P_STRING, (void *)&mailer, 0, NULL},
 {"from address", P_STRING, (void *)&from_address, FLAG_MANDATORY, NULL},
 {"smtp address", P_STRING, (void *)&smtp_address, 0, "127.0.0.1"},
 {"title", P_STRING, (void *)&title, 0, "JitterBug"},
 {"base url", P_STRING, (void *)&base_url, 0, NULL},
 {"base directory", P_STRING, (void *)&base_directory, FLAG_MANDATORY, NULL},
 {"chroot directory", P_STRING, (void *)&chroot_directory, 0, NULL},
 {"auth user", P_STRING, (void *)&auth_user, 0, NULL},
 {"guest gid", P_INTEGER, (void *)&guest_gid, FLAG_MANDATORY, NULL},
 {"guest uid", P_INTEGER, (void *)&guest_uid, FLAG_MANDATORY, NULL},
 {"uid", P_INTEGER, (void *)&uid, FLAG_MANDATORY, NULL},
 {"gid", P_INTEGER, (void *)&gid, FLAG_MANDATORY, NULL},
 {"display binary", P_BOOL, (void *)&display_binary, 0, NULL},
 {"background", P_STRING, (void *)&background, 0, "/images/back.gif"},
 {"group authentication", P_BOOL, (void *)&group_authentication, 0, NULL},
 {"search program", P_STRING, (void *)&search_program, 0, NULL},
 {"forward all", P_STRING, (void *)&forward_all, 0, NULL},
 {"forward public", P_STRING, (void *)&forward_public, 0, NULL},
 {"gzip path", P_STRING, (void *)&gzip_path, 0, "/usr/bin/gzip"},
 {"reply strings", P_STRING, (void *)&reply_strings, 0, NULL},
 {"guest download", P_BOOL, (void *)&guest_download, 0, NULL},
 {"gzip download", P_BOOL, (void *)&gzip_download, 0, NULL},
 {"autopatch", P_STRING, (void *)&autopatch, 0, NULL},
 {"decoder", P_STRING, (void *)&decoder, 0, NULL},
 {"pr identifier", P_STRING, (void *)&pr_identifier,0, "PR#"},
	{NULL},
};


static int getbool(char *s)
{
	if (strcasecmp(s,"yes")==0) return 1;
	if (strcasecmp(s,"no")==0) return 0;
	if (strcasecmp(s,"true")==0) return 1;
	if (strcasecmp(s,"false")==0) return 0;

	if (!isdigit(s[0])) {
		fatal("\"%s\" is not an acceptable boolean\n", s);
	}

	return atoi(s);
}

static void param_set(char *label, char *v, int loaded)
{
	int i;

	for (i=0;params[i].label;i++)
		if (strcmp(label, params[i].label)==0) break;
	
	if (!params[i].label) fatal("unknown config option \"%s\"\n", label);

	switch (params[i].type) {
	case P_INTEGER:
		if (!isdigit(v[0])) {
			fatal("You must use an integer for \"%s\"\n", 
			      params[i].label);
		}
		*(int *)params[i].ptr = atoi(v);
		break;

	case P_BOOL:
		*(int *)params[i].ptr = getbool(v);
		break;

	case P_STRING:
		*(char **)params[i].ptr = strdup(v);
		if (!params[i].ptr) fatal("out of memory");
		break;
	}

	if (loaded) params[i].flags |= FLAG_LOADED;
}


static void parse_line(char *line)
{
	char *p;
	char *label, *v;

	trim_string(line,NULL, "\n");
	trim_string(line," ", " ");

	if (*line == '#') return;
	if (!*line) return;

	p = strchr(line,'=');
	if (!p) fatal("malformed config line \"%s\"\n", line);

	*p = 0;
	label = line;
	v = p+1;
	
	trim_string(label," ", " ");
	trim_string(v," ", " ");

	param_set(label, v, 1);
}


/* load a jitterbug config file */
void load_config(char *name)
{
	char *fname;
	char *p;
	FILE *f;
	char line[1024];
	int i;

	for (i=0;params[i].label;i++) {
		if (params[i].def) 
			param_set(params[i].label, params[i].def, 0);
	}

        /* only use last part of name */
        p = strrchr(name,'/');
#ifdef __OS2__
        if(p == NULL)
            p = strrchr(name,'\\');
#endif
	if (p) name = p+1;

	fname = (char *)malloc(strlen(CONFIG_DIRECTORY)+strlen(name)+2);
	if (!fname) fatal("out of memory");
	
	strcpy(fname, CONFIG_DIRECTORY);
	strcat(fname,"/");
        strcat(fname,name);

#ifdef __OS2__
        if((p = strrchr(fname, '.')) != NULL)
            strcpy(p, ".cfg");
#endif
	f = fopen(fname,"r");
	if (!f) fatal("cannot open config file %s : %s\n", 
		      fname, strerror(errno));
	
	free(fname);

	while (fgets(line, sizeof(line)-1, f)) {
		line[sizeof(line)-1] = 0;
		parse_line(line);
	}

	fclose(f);

	for (i=0;params[i].label;i++) {
		if ((params[i].flags & FLAG_MANDATORY) && 
		    !(params[i].flags & FLAG_LOADED))
			fatal("mandatory option \"%s\" not set\n",
			      params[i].label);
	}
}


char *lp_mailer(void)
{
	return mailer;
}

char *lp_incoming(void)
{
	return incoming;
}

char *lp_from_address(void)
{
	return from_address;
}

char *lp_smtp_address(void)
{
	return smtp_address;
}

char *lp_title(void)
{
	return title;
}

char *lp_base_directory(void)
{
	return base_directory;
}

char *lp_chroot_directory(void)
{
	return chroot_directory;
}

char *lp_auth_user(void)
{
	return auth_user;
}

int lp_guest_gid(void)
{
	return guest_gid;
}

int lp_guest_uid(void)
{
	return guest_uid;
}

int lp_gid(void)
{
	return gid;
}

int lp_uid(void)
{
	return uid;
}

int lp_display_binary(void)
{
	return display_binary;
}

int lp_guest_download(void)
{
	return guest_download;
}

int lp_gzip_download(void)
{
	return gzip_download;
}

char *lp_background(void)
{
	return background;
}

int lp_group_authentication(void)
{
	return group_authentication;
}

char *lp_search_program(void)
{
	return search_program;
}

char *lp_forward_all(void)
{
	return forward_all;
}

char *lp_forward_public(void)
{
	return forward_public;
}

char *lp_gzip_path(void)
{
	return gzip_path;
}

char *lp_reply_strings(void)
{
	return reply_strings;
}

char *lp_pr_identifier(void)
{
	return pr_identifier; 
}


char *lp_autopatch(void)
{
	return autopatch;
}

char *lp_decoder(void)
{
	return decoder;
}

char *lp_base_url(void)
{
	return base_url;
}
