/* 
   some simple CGI helper routines
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
 * $Log: cgi.c,v $
 * Revision 1.2  2000/04/25 19:52:09  harald
 * First OS/2 release
 *
 */

#include "jitterbug.h"

#define MAX_VARIABLES 10000

struct var {
	char *name;
	char *value;
};

static struct var variables[MAX_VARIABLES];
static int num_variables;

static void unescape(char *buf)
{
	char *p=buf;

	while ((p=strchr(p,'+')))
		*p = ' ';

	p = buf;

	while (p && *p && (p=strchr(p,'%'))) {
		int c1 = p[1];
		int c2 = p[2];

		if (c1 >= '0' && c1 <= '9')
			c1 = c1 - '0';
		else if (c1 >= 'A' && c1 <= 'F')
			c1 = 10 + c1 - 'A';
		else if (c1 >= 'a' && c1 <= 'f')
			c1 = 10 + c1 - 'a';
		else {p++; continue;}

		if (c2 >= '0' && c2 <= '9')
			c2 = c2 - '0';
		else if (c2 >= 'A' && c2 <= 'F')
			c2 = 10 + c2 - 'A';
		else if (c2 >= 'a' && c2 <= 'f')
			c2 = 10 + c2 - 'a';
		else {p++; continue;}
			
		*p = (c1<<4) | c2;

		memcpy(p+1, p+3, strlen(p+3)+1);
		p++;
	}
}

char *unquote(char *buf)
{
	char *p;

	unescape(buf);

	p = buf;

	while (p && *p && (p=strchr(p,'&'))) {
		if (strncmp(p,"&lt;", 4)==0) {
			*p = '<';
			memmove(p+1, p+4, strlen(p+4)+1);
			continue;
		}
		if (strncmp(p,"&gt;", 4)==0) {
			*p = '>';
			memmove(p+1, p+4, strlen(p+4)+1);
			continue;
		}
		if (strncmp(p,"&amp;", 5)==0) {
			*p = '&';
			memmove(p+1, p+5, strlen(p+5)+1);
			continue;
		}
		p++;
	}
	
	return buf;
}


static char *grab_line(FILE *f, int *cl)
{
	char *ret;
	int i = 0;
	int len = 1024;

	ret = (char *)malloc(len);
	if (!ret) return NULL;
	

	while ((*cl)) {
		int c = fgetc(f);
		(*cl)--;

		if (c == EOF) {
			(*cl) = 0;
			break;
		}
		
		if (c == '\r') continue;

		if (strchr("\n&", c)) break;

		ret[i++] = c;

		if (i == len-1) {
			char *ret2;
			ret2 = (char *)realloc(ret, len*2);
			if (!ret2) return ret;
			len *= 2;
			ret = ret2;
		}
	}
	

	ret[i] = 0;
	return ret;
}

/***************************************************************************
  load all the variables passed to the CGI program. May have multiple variables
  with the same name and the same or different values. Takes a file parameter
  for simulating CGI invocation eg loading saved preferences.

  An optional prefix can also be specified, in which case this prefix will
  be prepended to all variable names
  ***************************************************************************/
void cgi_load_variables(FILE *f1, char *prefix)
{
	FILE *f = f1;
	static char *line;
	char *p, *s, *tok;
	int len;

#ifdef DEBUG_COMMENTS
	printf("<!== Start dump in cgi_load_variables() %s ==>\n",__FILE__);
#endif

	if (!f1) {
		f = stdin;
		p = getenv("CONTENT_LENGTH");
		len = p?atoi(p):0;
	} else {
		fseek(f, 0, SEEK_END);
		len = ftell(f);
		fseek(f, 0, SEEK_SET);
	}


	if (len > 0 && 
	    (f1 ||
	     ((s=getenv("REQUEST_METHOD")) && 
	      strcasecmp(s,"POST")==0))) {
		while (len && (line=grab_line(f, &len))) {
			p = strchr(line,'=');
			if (!p) continue;
			
			*p = 0;
			
			if (prefix) {
				variables[num_variables].name = 
					(char *)malloc(strlen(line)+strlen(prefix)+2);
				if (!variables[num_variables].name) continue;
				sprintf(variables[num_variables].name,"%s_%s",
					prefix, line);
			} else {
				variables[num_variables].name = strdup(line);
			}

			variables[num_variables].value = strdup(p+1);

			free(line);
			
			if (!variables[num_variables].name || 
			    !variables[num_variables].value)
				continue;

			unescape(variables[num_variables].value);
			unescape(variables[num_variables].name);

#ifdef DEBUG_COMMENTS
			printf("<!== POST var %s has value \"%s\"  ==>\n",
			       variables[num_variables].name,
			       variables[num_variables].value);
#endif
			
			num_variables++;
			if (num_variables == MAX_VARIABLES) break;
		}
	}

	if (f1) {
#ifdef DEBUG_COMMENTS
	        printf("<!== End dump in cgi_load_variables() ==>\n"); 
#endif
		return;
	}

	fclose(stdin);

	if ((s=getenv("QUERY_STRING"))) {
		for (tok=strtok(s,"&;");tok;tok=strtok(NULL,"&;")) {
			p = strchr(tok,'=');
			if (!p) continue;
			
			*p = 0;
			
			if (prefix) {
				variables[num_variables].name = 
					(char *)malloc(strlen(tok)+strlen(prefix)+2);
				if (!variables[num_variables].name) continue;
				sprintf(variables[num_variables].name,"%s_%s",
					prefix, tok);
			} else {
				variables[num_variables].name = strdup(tok);
			}

			variables[num_variables].value = strdup(p+1);

			if (!variables[num_variables].name || 
			    !variables[num_variables].value)
				continue;

			unescape(variables[num_variables].value);
			unescape(variables[num_variables].name);

#ifdef DEBUG_COMMENTS
                        printf("<!== Commandline var %s has value \"%s\"  ==>\n",
                               variables[num_variables].name,
                               variables[num_variables].value);
#endif						
			num_variables++;
			if (num_variables == MAX_VARIABLES) break;
		}

	}
#ifdef DEBUG_COMMENTS
        printf("<!== End dump in cgi_load_variables() ==>\n");   
#endif
}


/***************************************************************************
  find a variable passed via CGI
  Doesn't quite do what you think in the case of POST text variables, because
  if they exist they might have a value of "" or even " ", depending on the 
  browser. Also doesn't allow for variables[] containing multiple variables
  with the same name and the same or different values.
  ***************************************************************************/
char *cgi_variable(char *name)
{
	int i;

	for (i=0;i<num_variables;i++)
		if (strcmp(variables[i].name, name) == 0)
			return variables[i].value;
	return NULL;
}

/***************************************************************************
return a particular cgi variable
  ***************************************************************************/
char *cgi_vnum(int i, char **name)
{
	if (i < 0 || i >= num_variables) return NULL;
	*name = variables[i].name;
	return variables[i].value;
}

/***************************************************************************
  return the value of a CGI boolean variable.
  ***************************************************************************/
int cgi_boolean(char *name, int def)
{
	char *p = cgi_variable(name);

	if (!p) return def;

	return strcmp(p, "1") == 0;
}

/***************************************************************************
like strdup() but quotes < > and &
  ***************************************************************************/
char *quotedup(char *s)
{
	int i, n=0;
	int len;
	char *ret;
	char *d;

	if (!s) return strdup("");

	len = strlen(s);

	for (i=0;i<len;i++)
		if (s[i] == '<' || s[i] == '>' || s[i] == '&')
			n++;

	ret = malloc(len + n*6 + 1);

	if (!ret) return NULL;

	d = ret;

	for (i=0;i<len;i++) {
		if (!isspace(s[i]) && !isprint(s[i]) &&
		    !lp_display_binary()) {
			*d++ = '.';
			continue;
		}
		switch (s[i]) {
		case '<':
			strcpy(d, "&lt;");
			d += 4;
			break;

		case '>':
			strcpy(d, "&gt;");
			d += 4;
			break;

		case '&':
			strcpy(d, "&amp;");
			d += 5;
			break;

		default:
			*d++ = s[i];
		}
	}

	*d = 0;

	return ret;
}


/***************************************************************************
like strdup() but quotes a wide range of characters
  ***************************************************************************/
char *urlquote(char *s)
{
	int i, n=0;
	int len;
	char *ret;
	char *d;
	char *qlist = "\"\n\r'&<> \t+;";

	if (!s) return strdup("");

	len = strlen(s);

	for (i=0;i<len;i++)
		if (strchr(qlist, s[i])) n++;

	ret = malloc(len + n*2 + 1);

	if (!ret) return NULL;

	d = ret;

	for (i=0;i<len;i++) {
		if (strchr(qlist,s[i])) {
			sprintf(d, "%%%02X", (int)s[i]);
			d += 3;
		} else {
			*d++ = s[i];
		}
	}

	*d = 0;

	return ret;
}


/***************************************************************************
like strdup() but quotes " characters
  ***************************************************************************/
char *quotequotes(char *s)
{
	int i, n=0;
	int len;
	char *ret;
	char *d;

	if (!s) return strdup("");

	len = strlen(s);

	for (i=0;i<len;i++)
		if (s[i] == '"')
			n++;

	ret = malloc(len + n*6 + 1);

	if (!ret) return NULL;

	d = ret;

	for (i=0;i<len;i++) {
		switch (s[i]) {
		case '"':
			strcpy(d, "&quot;");
			d += 6;
			break;

		default:
			*d++ = s[i];
		}
	}

	*d = 0;

	return ret;
}


/***************************************************************************
quote spaces in a buffer
  ***************************************************************************/
void quote_spaces(char *buf)
{
	while (*buf) {
		if (*buf == ' ') *buf = '+';
		buf++;
	}
}

#ifndef __OS2__
/***************************************************************************
setup for gzip encoding
  ***************************************************************************/
void cgi_start_gzip(void)
{
	int fd[2];

	fflush(stdout);

	if (pipe(fd) != 0) fatal("failed to create pipe");

	if (fork()) {
		if (dup2(fd[0],0) != 0) fatal("dup2 failed\n");
		if (fd[0] != 0) close(fd[0]);
		close(fd[1]);
		execl(lp_gzip_path(), "gzip", "-f", NULL);
		fatal("execl of %s failed\n", lp_gzip_path());
	}

	if (dup2(fd[1],1) != 1) {
		fatal("dup2 failed\n");
	}

	if (fd[1] != 1) close(fd[1]);
	close(fd[0]);
}
#endif
