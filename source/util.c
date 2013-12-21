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
 * $Log: util.c,v $
 * Revision 1.2  2000/04/25 19:52:11  harald
 * First OS/2 release
 *
 */

#include "jitterbug.h"

/* lowercase a string */
void strlower(char *str)
{
	while (*str) {
		if (isupper(*str)) *str = tolower(*str);
		str++;
	}
}


/* check if a file is a directory */
int is_directory(char *dir)
{
        struct stat st;
        if (stat(dir, &st)) return 0;
        return S_ISDIR(st.st_mode);
}

/* check if a file exists */
int file_exists(char *fname, struct stat *st)
{
	struct stat st2;
	if (!st) st = &st2;
	if (stat(fname, st)) return 0;
	return 1;
}

/* recursively find first matching file from a specified point in a hierachy.
   return the matching filename (full pathname) */
char *find_file(char *dir, char *name)
{
	char **dir_entries;
	char *ret = NULL;
	int i;
	struct stat st;
	char *fname;

	fname = (char *)malloc(strlen(dir)+strlen(name)+2);
	if (!fname) fatal("out of memory\n");
	if (strcmp(dir,"/")==0) {
	         sprintf(fname,"/%s",name);
	} else {
	         sprintf(fname,"%s/%s", dir, name);
	}
        if (stat(fname,&st)==0) {
		return fname;
	}
	free(fname);
	
	dir_entries = load_dir_list(dir,is_directory);
	for (i=0;dir_entries[i];i++) {
		char *dname = (char *)malloc(strlen(dir)+strlen(dir_entries[i])+2);
		if (!dname) return NULL;
		sprintf(dname,"%s/%s", dir, dir_entries[i]);
		
		ret = find_file(dname,name);
		free(dname);
		if (ret) {
			free_list(dir_entries);
			return ret;
		}
	}

	free_list(dir_entries);

	return NULL;
}


/* wrap a string */
char *wrap(char *s, int ncols)
{
	char *ret = s;
	int x=0;
	char *last_space=NULL;

	if (!ret) return NULL;

	while (*s) {
		switch (*s) {
		case '\n':
			x=0;
			last_space = NULL;
			break;

		case ' ':
		case '\t':
			last_space = s;
			x++;
			break;

		default:
			x++;
			break;
		}
		if (x > ncols && last_space && x-(int)(s - last_space) > ncols/4) {
			*last_space = '\n';
			x = (s - last_space);
			last_space = NULL;
		}
		s++;
	}
	return ret;
}



/* extract an email address from a from line. Note the peculiar processing
   of the < and > characters because the from line has already been through
   quotedup() */
char *extract_address(char *s)
{
	char *p1, *p2, *p3;
	char buf[100];
	if (strlen(s) > sizeof(buf)-1) return strdup(s);

	/* first try for something with <> around it */
	p1 = strstr(s,"&lt;");
	if (p1) {
		p1 += 4;
		p2 = strstr(p1,"&gt;");
		if (p2) {
			while ((p3 = strstr(p1,"&lt;")) && (p3 < p2)) {
				p1 = p3 + 4;
			}
			memcpy(buf, p1, p2-p1);
			buf[(int)(p2-p1)] = 0;
			return strdup(buf);
		}
	}

	/* first try for something with <> around it */
	p1 = strstr(s,"<");
	if (p1) {
		p1 += 1;
		p2 = strstr(p1,">");
		if (p2) {
			while ((p3 = strstr(p1,">")) && (p3 < p2)) {
				p1 = p3 + 1;
			}
			memcpy(buf, p1, p2-p1);
			buf[(int)(p2-p1)] = 0;
			return strdup(buf);
		}
	}

	/* now try for anything with an @ in it */
	p1 = strchr(s,'@');
	if (p1) {
		while (p1 > s && !isspace(*p1)) p1--;
		if (isspace(*p1)) p1++;
		p2 = p1+1;
		while (*p2 && !isspace(*p2)) p2++;
		memcpy(buf,p1, p2-p1);
		buf[(int)(p2-p1)] = 0;
		return strdup(buf);
	}
	
	/* otherwise grab the first thing */
	p1 = s;
	while (*p1 && isspace(*p1)) p1++;
	p2 = p1;
	while (*p2 && !isspace(*p2)) p2++;
	memcpy(buf,p1, p2-p1);
	buf[(int)(p2-p1)] = 0;

	return strdup(buf);
}

/* compare two names. This is used with qsort(3) on directories or usernames*/
int namecmp(const void *n1p, const void *n2p)
{
	char **n1 = (char **)n1p;
	char **n2 = (char **)n2p;
	int i1 = atoi(*n1);
	int i2 = atoi(*n2);
	if (i1 != i2) return i1-i2;

	return strcmp(*n1, *n2);
}


/* pull a specified header out of a piece of email. Case insensititve: RFC822 */
char *getmailheader(char *raw,char *head, int quote)
{
	char *p1, *p2;
	char c;

	/* look for linefeeds - all headers must start at the left margin */
	p1 = raw;
	do {
		if (p1 != raw) p1++;

		if (strncasecmp(p1, head, strlen(head))==0) break;
	} while (p1 && *p1 && (p1 = strchr(p1+1,'\n')));
	if (!p1) return NULL;

	if (strncasecmp(p1, head, strlen(head))) return NULL;

	p1 += strlen(head);
	while (*p1 && ((*p1) == ' ' || (*p1) == '\t')) p1++;
	p2 = p1;
	while (*p2 && (*p2) != '\n' && (*p2) != '\r') p2++;

	if (!(*p2)) return NULL;

	while (p2 > p1 && isspace(*p2))
		p2--;
	if (*p2 && !isspace(*p2))
		p2++;

	c = *p2;
	(*p2) = 0;
	if (quote)
		p1 = quotedup(p1);
	else
		p1 = strdup(p1);
	(*p2) = c;
	
	return p1;
}


/* return a string showing the current date and time */
char *timestring(void)
{
	struct tm *tm;
	static char tbuf[100];
	time_t t = time(NULL);

	tm = localtime(&t);

	strftime(tbuf,sizeof(tbuf)-1,"%c", tm);
	return tbuf;
}


/* load a file as a string */
char *load_file(char *fname, struct stat *st, int max_size)
{
	int fd;
	struct stat st1;
	char *ret;
#ifdef __OS2__
        int n;
#endif

	if (!st) st = &st1;

        fd = open(fname,O_RDONLY);
	if (fd == -1) return NULL;

	if (fstat(fd, st)) {
		close(fd);
                return NULL;
	}

	ret = (char *)malloc(st->st_size + 1);
	if (!ret) {
		close(fd);
                return NULL;
	}

	if (max_size == 0 || max_size > st->st_size)
		max_size = st->st_size;

#ifdef __OS2__
        if ((n = read(fd, ret, max_size)) <= 0) {
#else
        if (read(fd, ret, max_size) != max_size) {
#endif
		close(fd);
		free(ret);
                return NULL;
	}

#ifdef __OS2__
        ret[n] = 0;
#else
        ret[max_size] = 0;
#endif
	close(fd);
        return ret;
}


/* save a file from a string */
void save_file(char *fname, char *s)
{
	int fd;

	fd = open(fname,O_WRONLY|O_CREAT|O_TRUNC,0644);
	if (fd == -1) return;

	while (*s) {
		if (*s == '\r') {
			s++; continue;
		}
		if (write(fd, s, 1) != 1)
			fatal("write failed to %s\n", fname);
		s++;
	}

	close(fd);
}

/* show a selection list, including a blank element */
void select_list(char **list, char *vname, char *def)
{
	int i;

	printf("<select name=%s>\n", vname);
        printf("<option %s value=\"\">\n", (def && *def)?"":"SELECTED");
	for (i=0;list && list[i];i++) {
		char *s = quotequotes(list[i]);
		printf("<option %s value=\"%s\">%s\n", 
		       (def && strcmp(def, list[i])==0) ? "SELECTED":"",
		       s, s);
		free(s);
	}
	printf("</select>\n");
}

/* return 1 if this is a valid name for a user */ 
int valid_user(char *username)
{
        int i;

        if (!username || !*username) return 0;

	i = strlen(username);
        if (i > MAX_USERNAME_LEN || i < 2) return 0;

	while (*username) {
		if (!isalnum(*username)) return 0;
		username++;
	}

        return 1;
}

int in_list(char **list, char *s)
{
	int i;
	if (!list) return 0;

	for (i=0;list[i];i++)
		if (strcasecmp(list[i], s)==0) return 1;
	return 0;
}


/* the root directory of the bug tracking system - changes
   after the chroot */
char *root_directory(void)
{
	static char buf[1024];
	char *p;
	extern int done_chroot;

        p = lp_chroot_directory();

	if (done_chroot || !p || !*p) {
		check_overflow(strlen(lp_base_directory())+10, sizeof(buf));
		strcpy(buf, lp_base_directory());
	} else {
		check_overflow(strlen(p)+strlen(lp_base_directory())+10, 
			       sizeof(buf));
		sprintf(buf,"%s%s", p, lp_base_directory());
	}

	if (buf[strlen(buf)-1] == '/') buf[strlen(buf)-1] = 0;
	return buf;
}


/* work out a email address for a jitterbug user - if not found then
   return the input string 

   The user list is scanned to find a matching username then this username
   is used to open the user preferences and extract the email variable
*/
char *user_address(char *user)
{
	FILE *f;
	char buf[1000], var[100];
	char *p;
	static char **user_list;
	int i;

	if (!valid_user(user)) return user;

	check_overflow(strlen(user) + 10, sizeof(var));
	sprintf(var, "%s_email", user);

	if ((p = cgi_variable(var)) && *p)
		return p;

	if (!user_list) {
		check_overflow(strlen(root_directory()) + 10, sizeof(buf));
		sprintf(buf,"%s/users", root_directory());
		user_list = load_file_list(buf,valid_user);
	}

	if (!user_list) {
		return user;
	}

	for (i=0;user_list[i];i++)
		if (strcasecmp(user_list[i], user)==0) break;
	if (!user_list[i]) return user;

	check_overflow(strlen(root_directory()) + strlen(user_list[i]) + 10, 
		       sizeof(buf));
	sprintf(buf,"%s/%s.prefs", root_directory(), user_list[i]);

	f = fopen(buf,"r");
	if (!f) return user;

	cgi_load_variables(f, user);

	fclose(f);

	if ((p = cgi_variable(var)) && *p)
		return p;

	return user;
}

/* count the followups to a message */
int count_followups(char *name, time_t *last)
{
	struct stat st;
	char buf[1000];
	int i;
	if (last) *last = 0;

	for (i=1;i<MAX_REPLIES;i++) {
		check_overflow(strlen(name)+30, sizeof(buf));
		sprintf(buf,"%s.followup.%d", name, i);
		if (stat(buf,&st)) break;
		if (last) *last = st.st_mtime;
	}
	return i-1;
}

/* count the replies to a message */
int count_replies(char *name, time_t *last)
{
	struct stat st;
	char buf[100];
	int i;
	if (last) *last = 0;

	for (i=1;i<MAX_REPLIES;i++) {
		check_overflow(strlen(name)+20, sizeof(buf));
		sprintf(buf,"%s.reply.%d", name, i);
		if (stat(buf,&st)) break;
		if (last) *last = st.st_mtime;
	}
	return i-1;
}



/* return 1 if this is a valid name for a message */
int valid_id(char *id)
{
	if (!id || !*id) return 0;
	if (strlen(id) > MAX_MESSAGEID_LEN) return 0;
	while (*id) {
		if (!isdigit(*id)) return 0;
		id++;
	}
	return 1;
}


/* load information about a message. */
int get_info(char *name, struct message_info *info, int max_size)
{
	char buf[1000];
	struct stat st;
	int ret;
	extern int guest;

	memset(info, 0, sizeof(*info));

	if (!valid_id(name)) return 0;

	check_overflow(strlen(name)+20, sizeof(buf));
	sprintf(buf,"%s.private", name);
	ret = stat(buf, &st);
	if (ret==0) {
		if (guest) return 0;
		info->private = 1;
	}

	info->raw = load_file(name, &st, max_size);

        if (!info->raw) return 0;

	info->size = st.st_size;
	info->from = getmailheader(info->raw, "From:", 1);
	info->subject = getmailheader(info->raw, "Subject:", 1);
	info->date = getmailheader(info->raw, "Date:", 1);

	if (info->subject && strstr(info->subject,PRIVATE)) {
		info->private = 1;
		if (guest) return 0;
	}

	if (!info->subject || !*info->subject)
		info->subject = strdup("none");

	if (!info->from || !*info->from)
	        info->from = strdup("unknown");

	if (!info->date || !*info->date)
	        info->date = strdup("unknown");

	if (!info->from || !info->subject || !info->date) {
	        fatal("out of memory in get_info");
	}

	info->loaded = 1;

	return 1;
}

#ifndef HAVE_MEMMOVE
/*******************************************************************
safely copies memory, ensuring no overlap problems.
this is only used if the machine does not have it's own memmove().
this is not the fastest algorithm in town, but it will do for our
needs.
********************************************************************/
 void *memmove(void *dest,void *src,int size)
{
  unsigned long d,s;
  int i;
  if (dest==src || !size) return(dest);

  d = (unsigned long)dest;
  s = (unsigned long)src;

  if ((d >= (s+size)) || (s >= (d+size))) {
    /* no overlap */
    memcpy(dest,src,size);
    return(dest);
  }

  if (d < s)
    {
      /* we can forward copy */
      if (s-d >= sizeof(int) && 
	  !(s%sizeof(int)) && !(d%sizeof(int)) && !(size%sizeof(int))) {
	/* do it all as words */
	int *idest = (int *)dest;
	int *isrc = (int *)src;
	size /= sizeof(int);
	for (i=0;i<size;i++) idest[i] = isrc[i];
      } else {
	/* simplest */
	char *cdest = (char *)dest;
	char *csrc = (char *)src;
	for (i=0;i<size;i++) cdest[i] = csrc[i];
      }
    }
  else
    {
      /* must backward copy */
      if (d-s >= sizeof(int) && 
	  !(s%sizeof(int)) && !(d%sizeof(int)) && !(size%sizeof(int))) {
	/* do it all as words */
	int *idest = (int *)dest;
	int *isrc = (int *)src;
	size /= sizeof(int);
	for (i=size-1;i>=0;i--) idest[i] = isrc[i];
      } else {
	/* simplest */
	char *cdest = (char *)dest;
	char *csrc = (char *)src;
	for (i=size-1;i>=0;i--) cdest[i] = csrc[i];
      }      
    }
  return(dest);
}
#endif

#ifndef HAVE_STRDUP
/****************************************************************************
duplicate a string
****************************************************************************/
 char *strdup(char *s)
{
	char *ret = NULL;
	if (!s) return(NULL);
	ret = (char *)malloc(strlen(s)+1);
	if (!ret) return(NULL);
	strcpy(ret,s);
	return(ret);
}
#endif


/* load a file with an extension */
char *load_ext(char *id, char *ext)
{
	char buf[100];
	check_overflow(strlen(id)+strlen(ext)+10, sizeof(buf));
	sprintf(buf,"%s.%s", id, ext);
	return load_file(buf, NULL, 0);
}


/* remove jusk from front and back of a string */
void trim_string(char *s,char *front,char *back)
{
	while (front && *front && strncmp(s,front,strlen(front)) == 0) {
		char *p = s;
		while (1) {
			if (!(*p = p[strlen(front)]))
				break;
			p++;
		}
	}
	while (back && *back && strlen(s) >= strlen(back) && 
	       (strncmp(s+strlen(s)-strlen(back),back,strlen(back))==0)) {
		s[strlen(s)-strlen(back)] = 0;
	}
}

void check_overflow(int len, int space)
{
	if (len > space) fatal("buffer overflow %d %d\n", len, space);
}

char *d_name(struct dirent *di)
{
#if HAVE_BROKEN_READDIR
	return (di->d_name - 2);
#else
	return di->d_name;
#endif
}

int match_string(char *s, char *list)
{
	char *tok;
	if (!list || !s) return 0;
	list = strdup(list);

	for (tok=strtok(list," ,\t"); tok; tok=strtok(NULL," ,\t")) {
		if (strstr(s, tok)) {
			free(list);
			return 1;
		}
	}
	free(list);
	return 0;
}

char *nth_line(char *s, int n)
{
	while (s && n--) {
		s = strchr(s,'\n');
		if (s) s++;
	}
	return s;
}

/* a version of getenv that returns (NULL) if the variable does not exist */
char *getenv_null(char *ename)
{
	char *s = getenv(ename);
	if (!s) return "(NULL)";
	return s;
}
