/* 
   The Jitterbug report tracking system

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



   jitterbug.c, the main cgi-bin program. This is self-contained: once
   mail is placed in directories accessible to this program according
   to the documentation everything else is handled here.

*/
/*
 * Modified for OS/2 by harald.kipp@egnite.de.
 * All these additional modifications are public domain.
 *
 * $Log: jitterbug.c,v $
 * Revision 1.2  2000/04/25 19:52:09  harald
 * First OS/2 release
 *
 */

#include "jitterbug.h"

static struct message_info zero_info;

static char **main_dir_list;
static char **user_list;

enum mtype {MTYPE_ALL=0, MTYPE_PENDING=1, MTYPE_REPLIED=2, MTYPE_UNREPLIED=3};

/* Headers that always get displayed. Must be stored in lowercase. */
static char *main_headers[] = {"from:", "to:", "date:", "cc:", "subject:", NULL};

/* markers that start a URL */
static char *url_markers[] = {"http://", "ftp://", 
			      "mailto:", "https://", NULL};


/* are they logged in as a guest? */
int guest=0;

int done_chroot = 0;

/* global cgi variables -- reflect changes in dump_globals() for debugging*/
static char *directory;
static char *directoryq; /* quoted form */
static char *expression="";
char *user="";
static int page;
static int selectid;
static int case_sensitive;
static int messagetype = MTYPE_ALL;
static int numquotelines;
static int addsignature;
static int fullheaders;

/* these are the user preferences -- reflect changes in dump_globals() */
static char *textfont = "";
static char *editfont = "";
static char *signature = "";
static char *email = "";
static char *fullname = "";
static char *bgcolour = "";
static char *textcolour = "";
static char *linkcolour = "";
static char *vlinkcolour = "";
static char *alinkcolour = "";
static int notes_x = 85;
static int notes_y = 10;
static int mail_x = 85;
static int mail_y = 20;
static int messages_per_screen = 10;
static int dir_cols = 4;
static int max_message_size = 5000;
static int gzip_encoding = 0;
static int notes_lines=3;

static char *download_extension(void)
{
	if (gzip_encoding || lp_gzip_download()) 
		return GZIP_DOWNLOAD_EXTENSION;
	return DOWNLOAD_EXTENSION;
}

/* show some pre-formatted text */
static void preformatted(char *s)
{
	printf("<pre>");
	if (*textfont) {
		printf("<font %s>", textfont);
	}
	printf("%s", NS(s));
	if (*textfont) {
		printf("</font>");
	}
	printf("</pre>\n");
}


/* display a file as html. This is used for various info files
   in the system. Set html to 1 if the file contains html codes */
static int display_file(char *fname1, int html)
{
	char *data;
	char *fname = fname1;
	char *dir;

	if (*fname == '/') {
		dir = root_directory();
                fname = (char *)malloc(strlen(dir) + strlen(fname1) + 2);
		sprintf(fname,"%s%s", dir, fname1);
	}

	if (getuid() == 0) return 0;

	data = load_file(fname, NULL, 0);
	if (!data) {
		if (fname != fname1) free(fname);
		return 0;
	}

	if (html) {
		printf("%s\n", NS(data));
	} else {
		char *s = quotedup(data);
		preformatted(s);
		free(s);
	}

	if (fname != fname1) free(fname);
	free(data);
	return 1;
}	



/* display an email in a reasonable way */
static void display_email(char *fname)
{
	char *s, *p, *q, c;
	char *data = load_file(fname, NULL, 0);
	int i, len, remaining=0;
	if (!data) return;
	
	s = quotedup(data);
	if (!s) return;
	free(data);
	data = s;

	if (!fullheaders) {
		printf("<b><pre>");
		if (*textfont) {
			printf("<font %s>", textfont);
		}

		while ((p = strchr(s,'\n'))) {
			if (*s == '\n') {
				s++;
				break;
			}
			*p = 0;

			q = strdup(s); 
			if (!q) fatal("strdup out of memory in display_email");
			strlower(q); /*rfc822 says any case ok*/

			for (i=0;main_headers[i];i++) 
				if (strncmp(q,main_headers[i], strlen(main_headers[i])) == 0)
					printf("%s\n", s);
			*p = '\n';
			free(q);
			s = p+1;
		}
		if (*textfont) {
			printf("</font>");
		}
		printf("</pre></b>\n");
	}

	wrap(s, WRAP_COLS);

	len = strlen(s);

	if (max_message_size && len > max_message_size) {
		remaining = len - max_message_size;
		s[max_message_size] = 0;
	}
	
	printf("<pre>");
	if (*textfont) {
		printf("<font %s>", textfont);
	}

        if (!strlen(s)) {
	    printf("\nWarning: no body text for this message\n");
	}

	while (s && *s) {
		for (i=0;url_markers[i];i++) 
			if (strncmp(url_markers[i],s, 
			    strlen(url_markers[i])) == 0) 
                        {
				p = s + strlen(url_markers[i]);
				
				while ((*p) &&
				       (isalnum(*p) || 
					strchr("/.@~-_#;&?:", *p))) {
					p++;
				}
				if (p[-1] == '.') p--;

				c = *p;
				*p = 0;

				printf("<A HREF=\"%s\">%s</A>", s, s);
				
				*p = c;
				s = p;
				break;
			}
		if (!url_markers[i]) {
			putchar(*s);
			if (*s) s++;
		}
	}
	
	if (*textfont) {
		printf("</font>");
	}
	printf("</pre>\n");

	if (remaining) {
		printf("<br><b>Message of length %d truncated</b> <input type=submit name=fullmessage value=\"Full Message\"><br>\n",
		       len);
	}

	free(data);
}


/* set the title */
void print_title(char *fmt, ...)
{
	va_list ap;  
	static int done_title;

	if (done_title) return;

	printf("Content-type: text/html\n");
#ifndef __OS2__
	if (gzip_encoding && !cgi_variable("preferences")) {
		printf("Content-Encoding: gzip\n\n");
		cgi_start_gzip();
        } else
#endif
		printf("\n");

	printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n");

	printf("<HTML>\n<HEAD>\n<TITLE>\n");

	va_start(ap, fmt);
#ifdef DEBUG_COMMENTS
	printf("Debugging in HTML comments: compiled %s %s\n",
	       __TIME__, __DATE__);	
#else
	vprintf(fmt, ap);
#endif
	va_end(ap);

	printf("</TITLE>\n</HEAD>\n");

	if (!display_file("/header.html", 1)) {
		if (*bgcolour || *textcolour || *linkcolour || *vlinkcolour ||
		    *alinkcolour) {
			printf("<BODY bgcolor=\"%s\" text=\"%s\" link=\"%s\" vlink=\"%s\" alink=\"%s\">", 
			       bgcolour, textcolour, linkcolour, vlinkcolour,
			       alinkcolour);
		} else {
			printf("<BODY background=\"%s\">\n\n", 
			       NS(lp_background()));
		}
	}

	done_title = 1;
}


/* finish off the page */
static void print_footer(void)
{
	display_file("/footer.html", 1);

	printf("\n</BODY>\n</HTML>\n");
}

#ifdef DEBUG_COMMENTS

static void dump_globals(void)
{
  char dummy[100]="";
  print_title(dummy);

  printf("<!== Start global variable dump in %s ==>\n",__FILE__);
  printf("<!== General globals: ==>\n");
  printf("<!==        directory=%s",NS(directory));
  printf("<!==        expression=%s",NS(expression));
  printf("<!==        user=%s",NS(user));
  printf("<!==        page=%d",page);
  printf("<!==        selectid=%d",selectid);
  printf("<!==        case_sensitive=%d",case_sensitive);
  printf("<!==        messagetype=%d",messagetype);
  printf("<!==        numquotelines=%d",numquotelines);
  printf("<!==        fullheaders=%d",fullheaders);
  printf("<!==        addsignature=%d",addsignature);

  printf("<!== User preferences: ==>\n");
  printf("<!==        textfont=%s",NS(textfont));
  printf("<!==        editfont=%s",NS(editfont));
  printf("<!==        signature=%s",NS(signature));
  printf("<!==        email=%s",NS(email));
  printf("<!==        fullname=%s",NS(fullname));
  printf("<!==        bgcolour=%s",NS(bgcolour));
  printf("<!==        textcolour=%s",NS(textcolour));
  printf("<!==        linkcolour=%s",NS(linkcolour));
  printf("<!==        vlinkcolour=%s",NS(vlinkcolour));
  printf("<!==        alinkcolour=%s",NS(alinkcolour));
  printf("<!==        notes_x=%d",notes_x);
  printf("<!==        notes_y=%d",notes_y);
  printf("<!==        mail_x=%d",mail_x);
  printf("<!==        mail_y=%d",mail_y);
  printf("<!==        messages_per_screen=%d",messages_per_screen);
  printf("<!==        dir_cols=%d",dir_cols);
  printf("<!==        gzip_encoding=%d",gzip_encoding);
  printf("<!==        notes_lines=%d",notes_lines);
  printf("<!==        max_message_size=%d",max_message_size);
  printf("<!== End global variable dump in %s ==>\n",__FILE__);
}
#endif

/* panic when something goes wrong */
void fatal(char *why, ...)
{
	va_list ap;  

        print_title("%s - fatal error", NS(lp_title()));
#ifdef DEBUG_COMMENTS 
	dump_globals();
#endif

	printf("The system encountered a fatal error<p><b>\n");

	printf("<pre>\n");
	va_start(ap, why);
	vprintf(why, ap);
	va_end(ap);
	printf("</pre>\n");

	printf("</b><p>\n");

	printf("The last error code was: %s\n<p>", strerror(errno));

	if (!guest) {
		printf("uid/gid=%d/%d\n<p>", getuid(), getgid());
	}

	print_footer();
	unlock_system();
	fflush(stdout);
	exit(0);
}


/* load the global variables */
void load_globals(void)
{
	char *p;
	if ((p=getenv("PATH_INFO"))) {
		if (*p == '/') p++;
		if (*p) {
			directory = strdup(p);
			unquote(directory);
			/* keep the quoted form available */
			directoryq = urlquote(p); 
		}
	}
	
	if ((p=cgi_variable("expression")) && *p) {
		expression = p;
	}

	if (guest) {
		user = "guest";
	} else {
		if ((p=cgi_variable("user")) && *p) {
		       user = p;
		       if (!valid_user(user)) fatal("invalid username");
		}

		if (lp_group_authentication()) {
			p = getenv("REMOTE_USER");
			if (!user_list)
				user_list = load_file_list("users",valid_user);
			if (p && valid_user(p) && in_list(user_list, p)) {
				user = p;
			}
		}
	}

	if ((p=cgi_variable("page"))) {
		page = atoi(p);
	}

	if ((p=cgi_variable("selectid"))) {
		selectid = atoi(p);
	}

	if ((p=cgi_variable("numquotelines"))) {
		numquotelines = atoi(p);
	}

	if ((p=cgi_variable("casesensitive"))) {
		case_sensitive = atoi(p);
	}

	if ((p=cgi_variable("addsignature"))) {
		addsignature = atoi(p);
	}

	if ((p=cgi_variable("fullheaders"))) {
		fullheaders = atoi(p);
	} 

	if ((p=cgi_variable("messagetype"))) {
		messagetype = atoi(p);
	}
}



/* a string of global variables for urls */
static char *url_globals(void)
{
	static char buf[1000];
	char *p=buf;

	if (strlen(expression) < 100 && expression && *expression) {
		sprintf(p,"expression=%s;",urlquote(expression));
		p += strlen(p);
	}

 	if (page) {
		sprintf(p, "page=%d;", page);
		p += strlen(p);
	}
 	if (user && *user) {
		sprintf(p, "user=%s;", user);
		p += strlen(p);
	}
	if (selectid) {
		sprintf(p, "selectid=%d;", selectid);
		p += strlen(p);
	}
	if (numquotelines) {
		sprintf(p, "numquotelines=%d;", numquotelines);
		p += strlen(p);
	}
	if (case_sensitive) {
		sprintf(p, "casesensitive=%d;", case_sensitive);
		p += strlen(p);
	}
	if (addsignature) {
		sprintf(p, "addsignature=%d;", addsignature);
		p += strlen(p);
	}
	if (fullheaders) {
		sprintf(p, "fullheaders=%d;", fullheaders);
		p += strlen(p);
	}
	if (messagetype) {
		sprintf(p, "messagetype=%d;", (int)messagetype);
		p += strlen(p);
	}

	if (p != buf) {
		p--;
		*p = 0;
	}

	if (p == buf) {
		return "null=0";
	}

	quote_spaces(buf);
	return buf;
}


/* setup globals as hidden variables */
void hidden_globals(void)
{
	if (*expression)
		printf("<input type=hidden name=expression value=\"%s\">\n",
		       quotequotes(expression));
	if (user && *user)
		printf("<input type=hidden name=user value=\"%s\">\n",
		       user);
	if (page)
		printf("<input type=hidden name=page value=\"%d\">\n",
		       page);
	if (selectid)
		printf("<input type=hidden name=selectid value=\"%d\">\n",
		       selectid);
	if (numquotelines)
		printf("<input type=hidden name=numquotelines value=\"%d\">\n",
		       numquotelines);
	if (case_sensitive)
		printf("<input type=hidden name=casesensitive value=\"%d\">\n",
		       case_sensitive);
	if (addsignature)
		printf("<input type=hidden name=addsignature value=\"%d\">\n",
		       addsignature);
	if (fullheaders)
		printf("<input type=hidden name=fullheaders value=\"%d\">\n",
		       fullheaders);
	if (messagetype)
		printf("<input type=hidden name=messagetype value=\"%d\">\n",
		       (int)messagetype);
}


/* get the curent page being viewed as a string */
static char *current_page(void)
{
	static char buf[1000];
	char *s1 = getenv("SCRIPT_NAME");
	char *s2 = directoryq;
	if (!s2) return NS(s1);
	if (strlen(s1) + strlen(s2) > 900)
		fatal("string too long");
	sprintf(buf, "%s/%s", s1, s2);
	return buf;
}


/* show the standard search buttons */
static void show_search_buttons(void)
{
	char sid[20]="";

	printf("\n<hr>\n");
	       
	printf("<table border=0>\n");

	printf("<tr><td>Case sensitive:</td>\n");
	printf("<td><input type=radio name=casesensitive value=1 %s>yes&nbsp;\n",case_sensitive?"CHECKED":"");
	printf("<input type=radio name=casesensitive value=0 %s>no</td>\n",!case_sensitive?"CHECKED":"");

        printf("<td>Regular expression</td><td><input size=20 type=text name=expression value=\"%s\"></td></tr>\n",quotequotes(expression));

        printf("<tr><td>Message type:</td><td>\n");
	printf("<input type=radio name=messagetype value=\"%d\" %s>all&nbsp;\n",
	       MTYPE_ALL, messagetype==MTYPE_ALL?"CHECKED":"");
	printf("<input type=radio name=messagetype value=\"%d\" %s>pending&nbsp;\n",
	       MTYPE_PENDING, messagetype==MTYPE_PENDING?"CHECKED":"");
	printf("<input type=radio name=messagetype value=\"%d\" %s>replied&nbsp;\n",
	       MTYPE_REPLIED, messagetype==MTYPE_REPLIED?"CHECKED":"");
	printf("<input type=radio name=messagetype value=\"%d\" %s>unreplied&nbsp;\n",
	       MTYPE_UNREPLIED, messagetype==MTYPE_UNREPLIED?"CHECKED":"");
	printf("</td>\n");

	if (selectid)
          sprintf(sid,"%d", selectid);

	printf("<td>Select message id:<td><input size=5 type=text name=selectid value=\"%s\"></td></tr>\n",
	       sid);

	printf("</table>\n");
	printf("<input type=submit name=Filter Value=\"Select Messages\">\n");

	printf("<hr>\n");
}


static void not_as_guest(char *action)
{
	printf("<b>You may not perform the action %s as a guest user</b><p>", action);
}


/* this makes the whole system fairly secure. If anyone does gain access
   they can only get at the bug reports files */
static void do_chroot(void)
{
	char *p, *user;
	char *dir;
	time_t t;
	char buf[10];

	/* this looks pointless, but it is needed in order for the
	   C library on some systems to fetch the timezone info
	   before the chroot */
	t = time(NULL);
	localtime(&t);

	/* this also seems pointless - see slprintf.c for an explanation */
	slprintf(buf, sizeof(buf), "xx");

	dir = lp_chroot_directory();

	if (dir && *dir) {
                if (chdir(dir)) {
			fatal("failed to chdir(%s)", dir);
		}

		if (strcmp(dir,"/") && (chroot(dir) || chdir("/"))) {
			fatal("failed to chroot(%s)", dir);
                }
	}

	done_chroot = 1;

	dir = lp_base_directory();

        if (chdir(dir)) {
                char path[_MAX_PATH];
                fatal("failed to chdir(%s)", dir);
	}

	p = getenv("AUTH_TYPE");
	if (!p || !*p) {
		guest = 1;
	}
	
	user = lp_auth_user();
	if (user && *user) {
		p = getenv("REMOTE_USER");
		if (!p || strcmp(p, user)) {
			guest = 1;
		}
	}

	if (guest) {
		if (getgid() != lp_guest_gid() && setgid(lp_guest_gid())) {
			fatal("failed to set guest gid");
		}
		if (getuid() != lp_guest_uid() && setuid(lp_guest_uid())) {
			fatal("failed to set guest uid");
		}
	} else {
		if (getgid() != lp_gid() && setgid(lp_gid())) {
			fatal("failed to set gid to %d", lp_gid());
		}
		if (getuid() != lp_uid() && setuid(lp_uid())) {
			fatal("failed to set uid to %d", lp_uid());
		}
	}
}




/* quote a email - putting in > characters at the start of each line */
static char *quote_email(char *raw)
{
	int i, count;
	char *ret, *raw1;
	char *p;

	raw1 = raw = strdup(raw);
	if (!raw) fatal("strdup out of memory in quote_email");

	wrap(raw, WRAP_COLS);

	p = strstr(raw,"\n\n");
	if (p) raw = p+2;

	for (i=count=0;raw[i];i++)
		if (raw[i] == '\n') count++;
	
	ret = malloc(i + count*3 + 10);

	if (!ret) fatal("malloc out of memory in quote_email");

	p = ret;
	strcpy(p,"\n> ");
	p += 3;

	while (*raw) {
		if (*raw == '\n') {
			if (raw[1] != '>') {
				strcpy(p,"\n> ");
				p += 3;
			} else {
				strcpy(p,"\n>");
				p += 2;
			}
			raw++;
		} else {
			*p++ = *raw++;
		}
	}

	*p = 0;

	free(raw1);

	return ret;
}


/* display a editable text box */
static void textarea(char *name, int rows, int cols, char *s)
{
	if (*editfont) {
		printf("<font %s>", editfont);
	}
	printf("<textarea name=%s rows=%d cols=%d wrap=physical>%s</textarea>\n",
	       name, rows, cols, s);
	if (*editfont) {
		printf("</font>");
	}
}


/* free up an info structure loaded by get_info */
static void free_info(struct message_info *info)
{
	if (!info->loaded) return;

	if (info->from) free(info->from);
	if (info->subject) free(info->subject);
	if (info->date) free(info->date);
	if (info->raw) free(info->raw);
	info->loaded = 0;
}


/* get the time on the notes file */
static time_t notes_time(char *name)
{
	struct stat st;
	char buf[100];

	if (!valid_id(name)) return 0;
	
	sprintf(buf,"%s.notes", name);
	if (stat(buf, &st)) return 0;
	return st.st_mtime;
}



/* see if a file matches the a expresion */
static int search_file(char *name)
{
	char *r;
	char *expr = expression;
	int ret=0;

	if (lp_search_program()) {
		return external_search(name, expression);
	}

	r = load_file(name,NULL,max_message_size);
	if (!r) return 0;

	if (!case_sensitive) {
		strlower(r);
		expr = strdup(expression);
		strlower(expr);
	}

	if (exp_match(r, expr)) ret = 1;
	free(r);
	if (expr != expression) free(expr);

	return ret;
}



/* see if a message matches the a expresion. If it does then fill in
   the info structure */
static int search_message(char *name)
{
	char buf[100];
	int i;

	if (search_file(name))
		return 1;

	sprintf(buf,"%s.notes", name);

	if (search_file(buf))
		return 1;

	for (i=1;i<MAX_REPLIES;i++) {
		sprintf(buf,"%s.reply.%d", name, i);
		if (!file_exists(buf, NULL)) break;

		if (search_file(buf))
			return 1;
	}

	for (i=1;i<MAX_REPLIES;i++) {
		sprintf(buf,"%s.followup.%d", name, i);
		if (!file_exists(buf, NULL)) break;

		if (search_file(buf))
			return 1;
	}

	return 0;
}


/* see if a message matches the current search criterion. If it does
   then fill in the info structure */
static int match_message(char *name, struct message_info *info)
{
	if (!valid_id(name)) return 0;

	if (selectid) {
		return (atoi(name) == selectid);
	}

	if (messagetype == MTYPE_PENDING) {
		time_t t1, t2, t3;
		count_followups(name, &t1);
		count_replies(name, &t2);
		t3 = notes_time(name);
		if (t1 <= t2 || t1 <= t3) return 0;
	}

	if (messagetype == MTYPE_REPLIED) {
		if (count_replies(name, NULL) == 0) return 0;
	}

	if (messagetype == MTYPE_UNREPLIED) {
		if (count_replies(name, NULL)) return 0;
	}

	if (!*expression) return 1;

	if (search_message(name)) {
		if (!get_info(name, info, max_message_size)) return 0;
		
		return 1;
	}

	return 0;
}


/* count the messages in a directory that match the specified 
   regular expression */
static int count_messages(void)
{
	int i, count=0;
	char **list;

	list = load_dir_list(".",valid_id);
	if (!list) return 0;

	for (i=0;list[i];i++) {
		struct message_info info = zero_info;
		if (match_message(list[i],&info))
			count++;
		free_info(&info);
	}

	free_list(list);

	return count;
}


/* add a audit message */
void add_audit(char *name,char *msg, ...)
{
	va_list ap;
	char buf[1024];
	FILE *f;

	va_start(ap, msg);

	if (!guest)
		lock_system();

	check_overflow(strlen(name) + strlen(root_directory()) + 10, sizeof(buf));

	if (*name == '/')
		sprintf(buf,"%s%s.audit", root_directory(), name);
	else
		sprintf(buf,"%s.audit", name);

	f = fopen(buf, "a");
	if (f) {
		fprintf(f,"%s\t%s\t", timestring(), user);
		vfprintf(f,msg,ap);
		fprintf(f,"\n");
		fclose(f);
	}

	va_end(ap);

	if (!guest)
		unlock_system();

	if (!guest && !f) {
		fatal("failed to open audit file %s.audit", name);
	}
}


static void show_directories(void)
{
	int i;

	printf("<A HREF=\"%s?%s\">Up to top level</A><br>\n",
	       getenv_null("SCRIPT_NAME"), url_globals());

	for (i=0;main_dir_list[i];i++) {
		printf("<A HREF=\"%s/%s?%s\">%s</A>&nbsp;&nbsp;\n",
		       getenv_null("SCRIPT_NAME"),
		       urlquote(main_dir_list[i]), 
		       url_globals(),
		       main_dir_list[i]);
	}
	printf("<p><b>Logged in as %s</b><p>\n", user);
}



/* this displays the message audit trail */
static void view_audit(void)
{
	char *name = cgi_variable("auditid");
	char buf[100];
	struct message_info info = zero_info;
	if (!get_info(name, &info, 0)) fatal("can't get info in view_audit");

	show_directories();

	printf("<form method=POST action=\"%s\">\n", current_page());
	printf("<p>Audit trail of %s/%s \n", directory, name);
	printf("<input type=submit name=return value=\"View Message\"><p>\n");

	check_overflow(strlen(name)+10,sizeof(buf));

	sprintf(buf,"%s.audit", name);
	printf("<hr>\n");
	display_file(buf, 0);
	printf("<hr>\n");
	
	hidden_globals();
	printf("<input type=hidden name=id value=\"%s\">\n", name);	
	printf("</form>\n");
}


/* this displays the message autopatch output */
static void view_autopatch(void)
{
	FILE *f;
	char *name = cgi_variable("auditid");
	char *source = cgi_variable("sources");
	char buf[1000];
	int len;
	struct message_info info = zero_info;

	if (!get_info(name, &info, 0)) fatal("can't get info in view_autopatch");

	if (!source || !*source) fatal("you must select a source tree\n");

	show_directories();

	printf("<form method=POST action=\"%s\">\n", current_page());
	printf("<p>AutoPatch output for %s/%s\n<br>", directory, name);

	check_overflow(strlen(lp_autopatch()) + 
		       strlen(name)+strlen(source)+10,sizeof(buf));

	sprintf(buf,"%s %s %s", lp_autopatch(), source, name);

	printf("<hr>\n");

	f = popen(buf,"r");
	if (!f) fatal("failed to execute %s\n", buf);

	while ((len = fread(buf, 1, sizeof(buf), f)) > 0) {
		fwrite(buf, 1, len, stdout);
	}
	pclose(f);

	printf("<hr>\n");
	
	hidden_globals();
	printf("<input type=hidden name=id value=\"%s\">\n", name);	
	printf("</form>\n");
}


/* this displays the message autopatch output */
static void file_decodeview(void)
{
	FILE *f;
	char *name = cgi_variable("decodeview");
	char buf[1000];
	int len;
	struct message_info info = zero_info;

	if (!get_info(name, &info, 0)) fatal("can't get info in file_decodeview");

	show_directories();

	printf("<form method=POST action=\"%s\">\n", current_page());
	printf("<p>Decoded patch for %s/%s\n<br>", directory, name);

	check_overflow(strlen(lp_decoder()) + 
		       strlen(name)+10,sizeof(buf));

	sprintf(buf,"%s %s", lp_decoder(), name);

	printf("<hr>\n");

	f = popen(buf,"r");
	if (!f) fatal("failed to execute %s\n", buf);

	printf("<pre>\n");
	while ((len = fread(buf, 1, sizeof(buf), f)) > 0) {
		fwrite(buf, 1, len, stdout);
	}
	pclose(f);
	printf("</pre>\n");

	printf("<hr>\n");
	
	hidden_globals();
	printf("<input type=hidden name=id value=\"%s\">\n", name);	
	printf("</form>\n");
}


/* this displays the system audit trail */
static void view_system_audit(void)
{
	char *fname = "system.audit";

	show_directories();

	if (guest) {
		not_as_guest("view_system_audit");
		return;
	}

	printf("<form method=POST action=\"%s\">\n", current_page());
	printf("<p>System audit trail\n");

	printf("<hr>\n");
	display_file(fname, 0);
	printf("<hr>\n");
	
	hidden_globals();
	printf("</form>\n");
}

static char *sources_dir(void)
{
	static char buf[1024];
	check_overflow(strlen(root_directory()) + strlen(SOURCES_DIR) + 10,sizeof(buf));
	sprintf(buf,"%s/%s", root_directory(), SOURCES_DIR);
	return buf;
}


static void download_options(char *name)
{
	printf("<A HREF=\"%s?%s;download=%s/%s%s\">Download message</A><br>\n",
	       current_page(), url_globals(),
	       directoryq, name, download_extension());

	if (lp_decoder()) {
		printf("<A HREF=\"%s?%s;decode=1;download=%s/%s%s\">Download decoded patch</A>&nbsp;&nbsp;\n",
		       current_page(), url_globals(),
		       directoryq, name, download_extension());
		printf("<A HREF=\"%s?%s;decodeview=%s\">View decoded patch</A><br>\n",
		       current_page(), url_globals(), name);
	}
}


/* this displays the message view/edit screen */
static void view_message(char *name)
{
	struct message_info info = zero_info;
	char *s;
	int j, i;

	if (!get_info(name, &info, 0)) fatal("can't get info in view_message");

	show_directories();

	printf("<form method=POST action=\"%s\">\n", current_page());
	printf("<p>Viewing <a href=\"#themesg\">%s/%s</a>", directory, name);
	printf("<input type=submit name=refresh value=Refresh><br>\n");
	printf("Full headers <input type=checkbox name=fullheaders value=1 %s><br>",fullheaders?"CHECKED":"");

	{
		/*ensure checkbox always has a value - see doc*/
		int headers_saved = fullheaders;
		fullheaders = 0;
		hidden_globals();
		fullheaders = headers_saved;
	}
	printf("<input type=hidden name=id value=\"%s\">\n", name);	
	printf("</form>\n");


	printf("<b>From: %s<br>Subject: %s<br></b>\n",
	       NS(info.from), NS(info.subject));

	printf("<A HREF=\"%s?compose=%s;%s\">Compose reply</A><br>\n",
	       current_page(), name, url_globals());

	download_options(name);

#if MESSAGE_LINKS
	printf("<A HREF=\"%s?links=%s;%s\">View/edit linked messages</A><br>\n",
	       current_page(),name,url_globals());
#endif

	printf("<form method=POST action=\"%s\">\n", current_page());

	printf("<input type=hidden name=changeid value=\"%s\">\n", name);

	printf("Move To: <select name=moveto.%s>\n", name);
	for (j=0;main_dir_list[j];j++)
		printf("<option %s value=\"%s\">%s\n",
		       strcmp(main_dir_list[j], directory)?"":"SELECTED",
		       main_dir_list[j], main_dir_list[j]);
	printf("</select><br>\n");

	s = quotedup(load_ext(name, "notes"));
	wrap(s, WRAP_COLS);
	printf("<A HREF=\"#replies\">%d replies</A>: ",
	       i = count_replies(name, NULL));
	for (j = 1; j <= i; j++)
		printf("<A HREF=\"#reply%d\">%d</A> ", j, j);
	printf("<br>\n");
	printf("<A HREF=\"#followups\">%d followups</A>: ",
	       i = count_followups(name, NULL));
	for (j = 1; j <= i; j++)
		printf("<A HREF=\"#followup%d\">%d</A> ", j, j);
	printf("<p>\n");

	printf("Private message: <input type=radio name=private value=1 %s>yes&nbsp;\n",
	       info.private?"CHECKED":"");
	printf(" <input type=radio name=private value=0 %s>no<p>\n",
	       !info.private?"CHECKED":"");

	printf("Notes:<br>\n");
	textarea("notes", notes_y, notes_x, s);
	free(s);

	printf("<br>Notification: <input type=text size=30 name=msg_notification value=\"%s\">\n",
	       msg_notification(name));

	printf("<br><input type=submit name=msg_change Value=\"Submit Changes\">\n");
	printf("<input type=hidden name=auditid value=\"%s\">\n", name);

	printf("<input type=submit name=audit Value=\"View Audit Trail\"><br>\n");

	if (lp_autopatch()) {
		char **sources_list = load_dir_list(sources_dir(), NULL);
		printf("<br>AutoPatch: ");
		select_list(sources_list, "sources", NULL);
		printf("<input type=submit name=autopatch Value=\"Test Patch\"><br>\n");
	}

	hidden_globals();

	printf("</form>\n");

	printf("<form method=POST action=\"%s\">\n", current_page());
	hidden_globals();
	printf("<input type=hidden name=id value=\"%s\"><br>\n", name);

	printf("<a name=\"themesg\"</a>\n");
	display_email(name);

	i = j = 1;
	while (i<MAX_REPLIES || j<MAX_REPLIES) {
		char buf1[100];
		char buf2[100];
		struct stat st1, st2;
		int r1, r2;
		int doreply=0;

		check_overflow(strlen(name)+20, sizeof(buf1));

		sprintf(buf1,"%s.reply.%d", name, i);
		sprintf(buf2,"%s.followup.%d", name, j);

		r1 = stat(buf1, &st1);
		r2 = stat(buf2, &st2);
		if (r1 && r2) break;

		if (!r1 && !r2) {
			if (st1.st_mtime < st2.st_mtime) {
				doreply = 1;
			} else {
				doreply = 0;
			}
		} else if (!r1) {
			doreply = 1;
		} else {
			doreply = 0;
		}

		if (doreply) {
			if (i == 1) printf("<a name=replies></a>\n");
			printf("<hr>\n");
			printf("<a name=\"reply%d\"></a>\n", i);
			printf("<H3>Reply %d</H3>\n", i);
			printf("<A HREF=\"%s?resend=%s;id=%s;%s\">Resend</A><br>\n",
			       current_page(), buf1, name, url_globals());
			display_email(buf1);
			printf("<hr>\n");
			i++;
			continue;
		}

		if (j == 1) printf("<a name=followups></a>\n");
		printf("<hr>\n");
		printf("<a name=\"followup%d\"></a>\n", j);
		printf("<H3>Followup %d</H3>\n", j);
		printf("<A HREF=\"%s?compose=%s;followup=%d;%s\">Compose reply</A><br>\n",
		       current_page(), name, j, url_globals());

		download_options(buf2);

		display_email(buf2);
		printf("<hr>\n");
		j++;
	}

	printf("</form>\n");

	show_directories();
}


static void send_mail(char *from, char *to, char *cc, char *bcc, char *subject,
		      char *body, char *sig)
{
	int fd;

	fd = smtp_start_mail(from, to, cc, bcc, subject, strlen(body)+2000);
	if (fd == -1) fatal(smtp_error());

	if (subject) {
		smtp_write(fd, "X-Loop: %s\n", lp_from_address());
		smtp_write(fd, "\n");
	}
	
	smtp_write_data(fd, body);
	if (sig)
		smtp_write_data(fd, sig);
	if (smtp_end_mail(fd) == -1)
		fatal(smtp_error());
}
		


static void send_bug_report(void)
{
	char *to = lp_from_address();
	char *from = cgi_variable("from");
	char *subject = cgi_variable("subject");
	char *body = cgi_variable("body");
	char *privates = cgi_variable("private");
	int fd, private=0;
	int i;
	char *vname, *value;

	if (!to || !from || !*from || !body || !*body || 
	    !subject || !*subject) 
		fatal("you must fill in all fields");

	if (strlen(from) > 500 || strlen(to) > 500 || 
	    strlen(subject) > 500)
		fatal("internal overflow sending bug report");

	wrap(body, WRAP_COLS);

	fd = smtp_start_mail(from, to, NULL, NULL, NULL, strlen(body) + 2000);
	if (fd == -1) fatal(smtp_error());

	if (privates && atoi(privates))
		private = 1;

	smtp_write(fd,"From: %s\n", from);
	smtp_write(fd,"To: %s\n", to);
	smtp_write(fd,"Subject: %s%s\n", 
		   private?"PRIVATE: ":"",
		   subject);
	smtp_write(fd, "\n");

	for (i=0; (value=cgi_vnum(i, &vname)); i++) {
		if (strncmp(vname, "opt_", 4)) continue;

		smtp_write(fd, "%s: %s\n", vname+4, value);
	}

	smtp_write(fd, "Submission from: %s (%s)\n",
		   getenv_null("REMOTE_HOST"), getenv_null("REMOTE_ADDR"));
	if (!guest)
		smtp_write(fd,"Submitted by: %s\n", user);

	smtp_write(fd, "\n\n");

	smtp_write_data(fd, body);

	smtp_end_mail(fd);

	printf("<b>Submission successful</b><p>\n");
}


static void send_reply(void)
{
	int i;
	char *to = cgi_variable("to");
	char *from = cgi_variable("from");
	char *subject = cgi_variable("subject");
	char *cc = cgi_variable("cc");
	char *bcc = cgi_variable("bcc");
	char *body = cgi_variable("body");
	char *name = cgi_variable("id");
	FILE *f;

	if (guest) {
		not_as_guest("send_reply");
		return;
	}

	if (!to || !from || !body || !subject) fatal("no message to send");

	if (strlen(from) > 500 || strlen(to) > 500 || 
	    (cc && strlen(cc) > 500) || 
	    (bcc && strlen(bcc) > 500) || 
	    strlen(subject) > 500)
		fatal("internal overflow while sending message");


	wrap(body, WRAP_COLS);

	send_mail(from, to, cc, bcc, subject, body, 
		  addsignature?signature:NULL);

	lock_system();

	/* now save this reply in a .reply file */
	for (i=1;i<MAX_REPLIES;i++) {
		int fd;
		time_t t = time(NULL);
		char buf[100];

		check_overflow(strlen(name)+20, sizeof(buf));

		sprintf(buf,"%s.reply.%d", name, i);
		
		fd = open(buf,O_WRONLY|O_CREAT|O_EXCL, 0644);
		if (fd == -1) continue;

		add_audit(name, "sent reply %d", i);

		f = fdopen(fd, "w");
		if (!f) fatal("failed to fdopen in send_reply");
		fprintf(f,"From: %s\n", from);
		fprintf(f,"To: %s\n", to);
		fprintf(f,"Subject: %s\n", subject);
		fprintf(f,"Date: %s", ctime(&t));

		if (cc && *cc)
			fprintf(f,"CC: %s\n", cc);
		fprintf(f,"\n%s", body);
		if (addsignature && signature)
			fprintf(f,"%s", signature);
		fclose(f);
		close(fd);

		break;
	}

	unlock_system();

	printf("<b>Mail sent successfully</b><p>\n");
}


static void resend_reply(void)
{
	char *to;
	char *from;
	char *cc, *raw;
	char *name = cgi_variable("resend");
	char *id = cgi_variable("id");

	if (guest) {
		not_as_guest("resend_reply");
		return;
	}

	if (!name) fatal("no message to resend");

	raw = load_file(name, NULL, 0);
	if (!raw) fatal("can't open file in resend_reply");

	to = getmailheader(raw, "To:", 0);
	cc = getmailheader(raw, "CC:", 0);
	from = getmailheader(raw, "From:", 0);

	if (cc) {
		if (*cc == '<') cc++;
		if (cc[strlen(cc)-1] == '>') cc[strlen(cc)-1] = 0;
	}

	if (!to || !from) fatal("can't parse message in resend_reply");

	send_mail(from, to, cc, NULL, NULL, raw, NULL);

	add_audit(id, "resent %s", name);

	printf("<b>Mail resent successfully</b><p>\n");
}



/* work out the from address for a reply - it can come form
   ether the info field or the From field of a followup */
static char *get_from(struct message_info *info, char *name)
{
	char *r, *ret=NULL;
	char buf[100];
	char *followup;

	if (info->raw)
		ret = getmailheader(info->raw, "Reply-To:", 1);
	if (!ret)
		ret = info->from;

	ret = extract_address(ret);

	followup = cgi_variable("followup");
	if (!followup) 
		return ret;

	check_overflow(strlen(name) + strlen(followup) + 20,sizeof(buf));

	sprintf(buf,"%s.followup.%s", name, followup);
	r = load_file(buf, NULL, 0);
	if (!r) return ret;

	ret = getmailheader(r, "Reply-To:", 1);
	if (!ret)
		ret = getmailheader(r, "From:", 1);
	if (!ret)
		ret = getmailheader(r, "Return-Path:", 1);
	if (!ret) fatal("can't parse from address");

	free(r);

	return extract_address(ret);
}


/* work out what text to quote */
static char *get_quote(struct message_info *info, char *name)
{
	char *followup;
	char *ret;

	if (guest) return "";

	if (!cgi_variable("doquote"))
		return "";

	followup = cgi_variable("followup");
	if (followup) {
		char *r;
		char buf[100];
		check_overflow(strlen(name) + strlen(followup) + 20,sizeof(buf));
		sprintf(buf,"%s.followup.%s", name, followup);
		r = load_file(buf, NULL, 0);
		if (!r) fatal("can't load file in get_quote");
		ret = quote_email(r);
	} else {
		ret = quote_email(info->raw);
	}

	if (numquotelines) {
		int i=numquotelines+1;
		char *p=ret;
		while (i-- && p && *p) {
			p = strchr(p, '\n');
			if (p) p++;
		}
		
		if (p) *p = 0;
	}

	return ret;
}


static char *faq_dir(void)
{
	static char buf[1024];
	check_overflow(strlen(root_directory()) + strlen(FAQ_DIR) + 10,sizeof(buf));
	sprintf(buf,"%s/%s", root_directory(), FAQ_DIR);
	return buf;
}

/* work out what body to include in a mail - could be a quote of a FAQ */
static char *get_body(struct message_info *info, char *name)
{
	char *p;
	char fname[200];
	char *body = cgi_variable("body");

	if (!body) body = "";

	if (cgi_variable("doquote")) return get_quote(info, name);

	if (cgi_variable("loadfaq") && (p=cgi_variable("faq"))) {
		check_overflow(strlen(faq_dir())+strlen(p)+10, sizeof(fname));
		sprintf(fname,"%s/%s", faq_dir(), p);
		p = load_file(fname,NULL,0);
		if (p) return p;
	}

	if (cgi_variable("savefaq") && (p=cgi_variable("faq")) &&
	    !strstr(p,"..")) {
		check_overflow(strlen(faq_dir())+strlen(p)+10, sizeof(fname));
		sprintf(fname,"%s/%s", faq_dir(), p);
		save_file(fname,body);
		add_audit(SYSTEMFILE,"saved FAQ %s", p);
	}

	return body;
}

static void save_preferences(void)
{
	FILE *f;
	char buf[100];

	if (guest) {
		not_as_guest("save_preferences");
		return;
	}

	check_overflow(strlen(user)+10, sizeof(buf));
	sprintf(buf,"%s.prefs", user);

	f = fopen(buf,"w");
	if (!f) {
	  fatal("Can't open preferences file \"%s.prefs\" for write",user);
	}

	fprintf(f,"signature=%s&", urlquote(signature));
	fprintf(f,"email=%s&", urlquote(email));
	fprintf(f,"fullname=%s&", urlquote(fullname));
	fprintf(f,"notesx=%d&", notes_x);
	fprintf(f,"notesy=%d&", notes_y);
	fprintf(f,"mailx=%d&", mail_x);
	fprintf(f,"maily=%d&", mail_y);
	fprintf(f,"textfont=%s&", urlquote(textfont));
	fprintf(f,"editfont=%s&", urlquote(editfont));
	fprintf(f,"messagespscreen=%d&", messages_per_screen);
	fprintf(f,"dircols=%d&", dir_cols);
	fprintf(f,"gzipencoding=%d&", gzip_encoding);
	fprintf(f,"noteslines=%d&", notes_lines);
	fprintf(f,"maxmessagesize=%d&", max_message_size);
	fprintf(f,"bgcolour=%s&", bgcolour);
	fprintf(f,"textcolour=%s&", textcolour);
	fprintf(f,"linkcolour=%s&", linkcolour);
	fprintf(f,"vlinkcolour=%s&", vlinkcolour);
	fprintf(f,"alinkcolour=%s&", alinkcolour);

	fclose(f);
}


static void load_preferences(void)
{
	FILE *f;
	char buf[100];
	char *p;

	check_overflow(strlen(user)+10, sizeof(buf));
	sprintf(buf,"%s.prefs", user);

	f = fopen(buf,"r");
	if (f) cgi_load_variables(f, NULL);

	if (!f && guest) return;

	if ((p=cgi_variable("signature"))) {
		signature = p;
	}

	if ((p=cgi_variable("email"))) {
		email = p;
	}

	if ((p=cgi_variable("fullname"))) {
		fullname = p;
	}

	if ((p=cgi_variable("bgcolour"))) {
		bgcolour = p;
	}

	if ((p=cgi_variable("textfont"))) {
		textfont = p;
	}

	if ((p=cgi_variable("editfont"))) {
		editfont = p;
	}

	if ((p=cgi_variable("textcolour"))) {
		textcolour = p;
	}

	if ((p=cgi_variable("linkcolour"))) {
		linkcolour = p;
	}

	if ((p=cgi_variable("vlinkcolour"))) {
		vlinkcolour = p;
	}

	if ((p=cgi_variable("alinkcolour"))) {
		alinkcolour = p;
	}

	if ((p=cgi_variable("notesx"))) {
		notes_x = atoi(p);
		if (notes_x < 1) notes_x = 1;
	}

	if ((p=cgi_variable("notesy"))) {
		notes_y = atoi(p);
		if (notes_y < 1) notes_y = 1;
	}

	if ((p=cgi_variable("mailx"))) {
		mail_x = atoi(p);
		if (mail_x < 1) mail_x = 1;
	}

	if ((p=cgi_variable("maily"))) {
		mail_y = atoi(p);
		if (mail_y < 1) mail_y = 1;
	}

	if ((p=cgi_variable("messagespscreen"))) {
		messages_per_screen = atoi(p);
		if (messages_per_screen < 1) 
			messages_per_screen = 1;
	}

	if ((p=cgi_variable("dircols"))) {
		dir_cols = atoi(p);
		if (dir_cols < 1) dir_cols = 1;
	}

	if ((p=cgi_variable("gzipencoding"))) {
		gzip_encoding = atoi(p);
	}

	if ((p=cgi_variable("noteslines"))) {
		notes_lines = atoi(p);
		if (notes_lines < 1) notes_lines = 1;
	}

	if ((p=cgi_variable("maxmessagesize"))) {
		max_message_size = atoi(p);
		if (max_message_size < 1000) max_message_size = 1000;
	}

	if (f) fclose(f);
}


/* edit the user preferences */
static void edit_preferences(void)
{
	show_directories();

	printf("<form method=POST action=\"%s\">\n", current_page());

	printf("<p>Editing preferences for %s\n", user);
	printf("<input type=submit name=commit Value=Commit>\n");
	printf(" <input type=submit name=preferences Value=Refresh><br>\n");

	printf("<p><table border=0>\n");
	printf("<tr><td>Full name: </td><td><input type=text size=60 name=fullname value=\"%s\"></td></tr>\n", fullname);
	printf("<tr><td>Email address: </td><td><input type=text size=60 name=email value=\"%s\"></td></tr>\n", email);
	printf("<tr><td>gzip encoding: </td><td><input type=text size=5 name=gzipencoding value=\"%d\"></td></tr>\n", gzip_encoding);
	printf("<tr><td>notes lines: </td><td><input type=text size=5 name=noteslines value=\"%d\"></td></tr>\n", notes_lines);
	printf("<tr><td>Notes columns: </td><td><input type=text size=5 name=notesx value=\"%d\"></td></tr>\n", notes_x);
	printf("<tr><td>Notes rows: </td><td><input type=text size=5 name=notesy value=\"%d\"></td></tr>\n", notes_y);
	printf("<tr><td>Mailer columns: </td><td><input type=text size=5 name=mailx value=\"%d\"></td></tr>\n", mail_x);
	printf("<tr><td>Mailer rows: </td><td><input type=text size=5 name=maily value=\"%d\"></td></tr>\n", mail_y);
	printf("<tr><td>Messages/screen: </td><td><input type=text size=5 name=messagespscreen value=\"%d\"></td></tr>\n", messages_per_screen);
	printf("<tr><td>Directory columns: </td><td><input type=text size=5 name=dircols value=\"%d\"></td></tr>\n", dir_cols);
	printf("<tr><td>Maximum message size: </td><td><input type=text size=8 name=maxmessagesize value=\"%d\"></td></tr>\n", max_message_size);

	printf("<tr><td>Background Colour: </td><td><input type=text size=10 name=bgcolour value=\"%s\"></td></tr>\n", bgcolour);
	printf("<tr><td>Text Colour: </td><td><input type=text size=10 name=textcolour value=\"%s\"></td></tr>\n", textcolour);
	printf("<tr><td>Link Colour: </td><td><input type=text size=10 name=linkcolour value=\"%s\"></td></tr>\n", linkcolour);
	printf("<tr><td>Active Link Colour: </td><td><input type=text size=10 name=alinkcolour value=\"%s\"></td></tr>\n", alinkcolour);
	printf("<tr><td>Visited Link Colour: </td><td><input type=text size=10 name=vlinkcolour value=\"%s\"></td></tr>\n", vlinkcolour);
	printf("<tr><td>Text Font: </td><td><input type=text size=30 name=textfont value=\"%s\"></td></tr>\n", textfont);
	printf("<tr><td>Edit Font: </td><td><input type=text size=30 name=editfont value=\"%s\"></td></tr>\n", editfont);

	printf("</table>\n");

	printf("Signature:<br>\n");
	textarea("signature", 8, 80, signature);

	printf("<p>Editing preferences for %s\n", user);
	printf("<input type=submit name=commit Value=Commit>\n");
	printf(" <input type=submit name=preferences Value=Refresh>\n");

	hidden_globals();
	printf("</form>\n");
}


/* compose a bug report */
static void compose_bug_report(void)
{
	show_directories();

	printf("<form method=POST action=\"%s\">\n", current_page());

	display_file("/reportform.html", 1);

	hidden_globals();

	printf("</form>\n");
}

/* compose a reply to a message */
static void compose_reply(void)
{
	struct message_info info = zero_info;
	char *name = cgi_variable("compose");
	char *subject = cgi_variable("subject");
	char *to = cgi_variable("to");
	char *from = cgi_variable("from");
	char *cc = cgi_variable("cc");
	char *bcc = cgi_variable("bcc");
	char **faq_list=NULL;
	char *faq = cgi_variable("faq");
	char *newfaq = cgi_variable("newfaq");
	char *body = cgi_variable("body");
	char *p;

	if (guest) {
		not_as_guest("compose_reply");
		return;
	}

	if (!name) fatal("no message specified");

	if (!get_info(name, &info, 0)) fatal("can't get info");

	if (!to) to = get_from(&info, name);
	if (!from) {
		from = (char *)malloc(strlen(fullname) + 
				      strlen(lp_from_address()) + 10);
		if (!from) fatal("out of memory\n");
		sprintf(from,"%s <%s>", fullname, lp_from_address());
	}
	if (!cc) cc = "";
	if (!bcc) bcc = "";

	show_directories();

	printf("<form method=POST action=\"%s\">\n", current_page());

	printf("<input type=hidden name=id value=\"%s\">\n", name);

	printf("<p>Replying to %s/%s<p>\n", directory, name);

	printf("<table border=0>\n");
	printf("<tr><td>From: </td><td><input type=text size=60 name=from value=\"%s\"></td></tr>\n", from);
	printf("<tr><td>To: </td><td><input type=text size=60 name=to value=\"%s\"></td></tr>\n", to);
	printf("<tr><td>CC: </td><td><input type=text size=60 name=cc value=\"%s\"></td></tr>\n", cc);
	printf("<tr><td>BCC: </td><td><input type=text size=60 name=bcc value=\"%s\"></td></tr>\n", bcc);
	if (!subject) {
		if (strstr(info.subject, lp_pr_identifier())) {
			printf("<tr><td>Subject: </td><td><input type=text size=60 name=subject value=\"%s%s\"></td></tr>\n",
			       strncasecmp(info.subject,"Re:", 3) == 0?"":"Re: ",
			       quotequotes(info.subject));
		} else {
			printf("<tr><td>Subject: </td><td><input type=text size=60 name=subject value=\"%s%s (%s%s)\"></td></tr>\n",
			       strncasecmp(info.subject,"Re:", 3) == 0?"":"Re: ",
			       quotequotes(info.subject), lp_pr_identifier(), name);
		}
	} else {
		printf("<tr><td>Subject: </td><td><input type=text size=60 name=subject value=\"%s\"></td></tr>\n", quotequotes(subject));
	}

	printf("</table>\n");


        if (!is_directory(faq_dir()))
#ifdef __OS2__
            mkdir(faq_dir());
#else
            mkdir(faq_dir(),0755);
#endif

	if (cgi_variable("createfaq") && (p=cgi_variable("newfaq")) &&
	    !strstr(p,"..")) {
		char fname[200];
		check_overflow(strlen(faq_dir()) + strlen(p) + 10, sizeof(fname));
		sprintf(fname,"%s/%s", faq_dir(), p);
		save_file(fname,body?body:"");
		faq = newfaq;
		add_audit(SYSTEMFILE,"created FAQ %s", faq);
	}
	

	/* generate the FAQ controls */
	faq_list = load_dir_list(faq_dir(), NULL);
	printf("<br>FAQ: ");
	select_list(faq_list, "faq", faq);
	printf("<input type=submit name=loadfaq Value=Load>\n");
	printf("<input type=submit name=savefaq Value=Save>\n");
	printf("&nbsp; New FAQ: <input type=text size=15 name=newfaq>\n");
	printf("<input type=submit name=createfaq Value=Create>\n");
	printf("<br>");
	free_list(faq_list);


	printf("<input type=submit name=send Value=Send>\n");
	printf("<input type=submit name=doquote Value=Quote>\n");
	printf("&nbsp; Quote lines: <input type=text size=5 name=numquotelines Value=");
	if (numquotelines)	printf("%d>\n", numquotelines);	
	else printf("\"\">\n");
	printf("&nbsp; Add signature<input type=checkbox name=addsignature value=1 %s>\n",
	       addsignature?"CHECKED":"");

	printf("<br>");

	textarea("body", mail_y, mail_x, get_body(&info, name));

	printf("<br><input type=hidden name=compose value=\"%s\">\n", name);
	if (cgi_variable("followup"))
		printf("<input type=hidden name=followup value=\"%s\">\n", 
		       cgi_variable("followup"));

	if (faq) {
		printf("<input type=hidden name=faq value=\"%s\">\n", faq);
	}

	/* damn checkboxes - they never send a 0 answer so this is needed */
	addsignature = 0;

	hidden_globals();
	printf("</form>\n");
}

/* show the page selection links */
static void page_selection(int total)
{
	if (total <= messages_per_screen)
		return;

	printf("Select page (%d at a time): ", messages_per_screen);
	
	if (page > 0)
		printf("<A HREF=\"%s?page=%d;%s\">previous(%d)</A> \n",
		       current_page(), page-1,
		       url_globals(), page-1);

	if ((page+1)*messages_per_screen < total)
		printf("<A HREF=\"%s?page=%d;%s\">next(%d)</A> \n",
		       current_page(), page+1,
		       url_globals(), page+1);

	if (total > messages_per_screen && page > 0)
		printf("<A HREF=\"%s?page=%d;%s\">first</A> \n",
		       current_page(), 0,
		       url_globals());

	if (page < (total-1)/messages_per_screen)
		printf("<A HREF=\"%s?page=%d;%s\">last</A> \n",
		       current_page(), (total-1)/messages_per_screen,
		       url_globals());
}

/* format the notes with right number of lines */
static void format_notes(char *notes)
{
	int n = notes_lines;

	if (!notes || !*notes) {
		printf("none\n");
		return;
	}

	while (n-- && *notes) {
		char *p = strchr(notes, '\n');
		if (!p) {
			printf("%s\n", notes);
			return;
		}
		*p = 0;
		printf("%s<br>\n", notes);
		notes = p+1;
	}
	if (*notes) {
		printf("<b>...</b><br>\n");
	}
}

/* display all the mesages in a directory - this is really the guts
   of the system */
void display_dir(void)
{
	char **list;
	int i, j, n;
	int total, count;

	display_file("notes.html", 1);

	list = load_dir_list(".",valid_id);
	if (!list) fatal("failed to load dir");

	for (i=0;list[i];i++) ;
	total = i;

	show_directories();

	printf("<p><form method=POST action=\"%s\">\n", current_page());

	show_search_buttons();

	printf("<input type=submit name=\"dir_change\" Value=\"Submit Changes\">\n");
	printf("<input type=submit name=refresh Value=Refresh><br>\n");

	printf("<br>Notification: <input type=text size=30 name=\"dir_notification\" value=\"%s\">\n",
	       dir_notification(directory));

	printf("<p><b>%s has %d messages</b><p>\n", directory, total);
	if (*expression || selectid || messagetype)
		printf("<p><b>%d messages match search criterion</b><p>\n", 
		       (total=count_messages()));

	if (page*messages_per_screen >= total) {
		page = (total-1)/messages_per_screen;
	}


	page_selection(total);

	printf("<p>"
"<table width=\"100%%\" border=1>"
"<tr bgcolor=\"#00A020\"><th><font color=\"#FFFF00\">Id</font></th><th><font color=\"#FFFF00\">Summary</font></th><th><font color=\"#FFFF00\">Notes</font></th><th><font color=\"#FFFF00\">Move To</font></th>"
"</tr>"
"\n");

	for (count=i=0;list[i];i++) {
		struct message_info info = zero_info;
		char *notes;
		if (!valid_id(list[i])) continue;
		if (!match_message(list[i],&info)) continue;
		count++;
		if (count <= page*messages_per_screen || 
		    count > (page+1)*messages_per_screen)
			continue;		
		if (!get_info(list[i], &info, max_message_size)) continue;
		
		printf("<tr valign=top><td><A HREF=\"%s?id=%s;%s\">%s</A>\n",
		       current_page(), list[i], 
		       url_globals(),
		       list[i]);
		       
		n = count_replies(list[i], NULL);
		if (n) {
			printf("<br>%d replies\n", n);
		} 
		n = count_followups(list[i], NULL);
		if (n) {
			printf("<br>%d followups\n", n);
		} 
#if MESSAGE_LINKS
		n = count_links(list[i]);
		if (n) {
			printf("<br>%d links\n",n);
		}
#endif
		printf("</td>\n");

		printf("<td><b>%s</b><br>%s<br>%s</td>\n",
		       info.subject, info.from, info.date);
		notes = quotedup(load_ext(list[i], "notes"));
		printf("<td>");
		format_notes(notes);
		if (info.private)
			printf("<br><b>PRIVATE</b></td>");
		else
			printf("</td>");
		free(notes);
		printf("<td><select name=moveto.%s>\n", list[i]);
		for (j=0;main_dir_list[j];j++)
			printf("<option %s value=\"%s\">%s\n",
			       strcmp(main_dir_list[j], directory)?"":"SELECTED",
			       main_dir_list[j], main_dir_list[j]);
		printf("</select><br>\n");
		printf("</td></tr>\n");
		free_info(&info);
	}

	printf("</table><p>\n");

	printf("<input type=submit name=\"dir_change\" Value=\"Submit Changes\">\n");
	printf("<input type=submit name=refresh Value=Refresh><br>\n");

	page_selection(total);

	hidden_globals();
	printf("</form>\n");

	show_directories();
	free_list(list);
}


/* login to the system */
void login_page(void)
{
	char *p;

	if (!user || !*user)
		printf("Welcome to JitterBug version %s<p>\n",VERSION);

	printf("<form method=POST action=\"%s\">\n", current_page());
	printf("Select your username:\n");
	select_list(user_list, "user", user);
	if (!*user)
		printf("<input type=hidden name=enter value=enter>\n");
	printf("<input type=submit name=enter value=\"Enter System\"> \n");
	printf("<input type=submit name=preferences value=\"Edit Preferences\">\n");
	printf("<input type=submit name=sysaudit value=\"View System Log\"><br>\n");
	if  ((p=cgi_variable("user")) && !valid_user(p) && 
	     !cgi_variable("newuser"))
		printf("<b>Must enter a valid username!</b>\n");

	printf("<p>Or create a new user: \n");
	printf("<input type=text size=15 name=newuser> \n");
	printf("<input type=submit name=createuser value=\"Create User\"><br>\n");

	if  ((p=cgi_variable("newuser")) && (*p) && !valid_user(p))
		printf("<b>new username not valid!</b>\n");

	hidden_globals();
	printf("</form>\n");
}


/* find a specified message ID and display it */
static void find_id(void)
{
	struct message_info info = zero_info;
	char *name = cgi_variable("findid");

	if (!valid_id(name)) fatal("invalid message id \"%s\"\n", name);

	if ((directory = find_file(".",name)) == NULL) {
		fatal("error finding message %s", name);
	} else {
		char *p;

		if (strncmp(directory,"./",2) == 0) {
			directory += 2;
		}

		p = strrchr(directory,'/');
		if (!p) fatal("invalid directory in find_id\n");

		*p = '\0';

                if (chdir(directory)) fatal("can't change directory");

		directoryq = urlquote(directory);

		if (!get_info(name, &info, 0)) fatal("can't get info in find_id");

		if (guest && info.private) {
			print_title("%s - Message %s is marked PRIVATE", lp_title(), name);
			printf("<h2>Message %s is marked PRIVATE\n", name);
		} else {
			print_title("%s - Message %s", lp_title(), name);
			view_message(name);
		}
	}
}


/* display the main page which allows users to select what directory 
   to view */
void main_page(void)
{
	int i;

	printf("<form method=POST action=\"%s\">\n", current_page());

	if (guest) 
		display_file("/guestintro.html", 1);
	else
		display_file("/intro.html", 1);

	show_search_buttons();

	printf("<p>To view messages select a directory from the list below: \n");
	printf("<input type=submit name=refresh value=Refresh><br>\n");

	printf("<table border=0>\n");
	for (i=0;main_dir_list[i];i++) {
		chdir(main_dir_list[i]);
		printf("%s<td>&nbsp;<A HREF=\"%s/%s?%s\">%s</A></td><td>%d</td>%s\n",
		       (i%dir_cols)==0?"<tr>":"",
		       getenv_null("SCRIPT_NAME"),
		       urlquote(main_dir_list[i]), 
		       url_globals(),
		       main_dir_list[i],
		       count_messages(),
		       (i%dir_cols)==(dir_cols-1)?"</tr>":"");
		chdir("..");
	}
	if (i%dir_cols) 
		printf("</tr>");
	printf("</table><br>\n");

	if (!guest) {
		printf("<p>Or create a new directory:<br>\n");

		printf("<input type=text size=25 name=newdir>\n");
		printf("<input type=submit name=createdir value=Create>\n");
		printf("<p><hr>");
	}

	hidden_globals();
	printf("</form>\n");

	if (!guest) {
		login_page();
	}
}



/* allow the user to download a file */
static void file_download(char *fname)
{
	FILE *f;
	int len;
	char buf[1000];
	char *decode = cgi_variable("decode");

	if (guest && !lp_guest_download()) {
		fatal("guest download has been disabled\n");
	}

	if (strcmp(fname+strlen(fname)-strlen(download_extension()),
		   download_extension()))
		fatal("illegal filename");

	fname[strlen(fname)-strlen(download_extension())] = 0;

	if (fname[0] == '/') fatal("illegal filename %s\n", fname);

	if (strstr(fname,"..")) fatal("illegal filename %s\n", fname);

	if (!lp_decoder()) decode = NULL;

	if (decode) {
		sprintf(buf,"%s %s", lp_decoder(), fname);
		
		f = popen(buf,"r");
	} else {
		f = fopen(fname,"r");
	}

	if (!f) {
		fatal("unable to open file");
	}

	printf("Content-Type: application/octet-stream\n");
#ifndef __OS2__
	if (gzip_encoding || lp_gzip_download()) {
		printf("\n");
		cgi_start_gzip();
        } else
#endif
		printf("\n");

	fflush(stdout);

	while ((len=fread(buf, 1, sizeof(buf), f)) > 0) {
		write(1, buf, len);
	}
	
	if (decode) {
		pclose(f);
	} else {
		fclose(f);
	}
}


/* handle a submitted changes form, updating the notes for a message */
static void do_msg_changes(void)
{
	char *id = cgi_variable("changeid");
	char *notes = cgi_variable("notes");
	char *privates = cgi_variable("private");
	char *notification = cgi_variable("msg_notification");
	char *orig_notes;
	int private;
	char fname[100];
	struct message_info info = zero_info;

	if (guest) {
		not_as_guest("do_msg_changes");
		return;
	}

	lock_system();

	if (!id || !valid_id(id) || !get_info(id,&info,max_message_size)) 
		return;

	orig_notes = load_ext(id, "notes");

	if (notes) {
		wrap(notes, WRAP_COLS);

		if (!orig_notes || strcmp(notes, orig_notes)) {
			check_overflow(strlen(id)+10, sizeof(fname));
			sprintf(fname,"%s.notes", id);
			if (strlen(notes) == 0) {
				unlink(fname);
			} else {
				save_file(fname,notes);
			}
			
			if (orig_notes || strlen(notes) > 0) {
				add_audit(id, "changed notes");
				notify_msg(id,"%s changed notes\n", 
					   user);
				notify_dir(directory, id, "%s changed notes\n", 
					   user);
			}
		}
	}

	if (privates) {
		private = atoi(privates);
		if (private != info.private) {
			check_overflow(strlen(id)+20, sizeof(fname));
			sprintf(fname,"%s.private", id);
			if (private) {
				int fd = open(fname,O_WRONLY|O_CREAT|O_TRUNC, 0444);
				if (fd == -1) fatal("can't create private file");
				close(fd);
			} else {
				unlink(fname);
			}
			add_audit(id, "marked %s", private?"private":"public");
		}
	}

	if (notification && strcmp(notification, msg_notification(id))) {
		check_overflow(strlen(fname)+10, sizeof(fname));
		sprintf(fname,"%s.notify", id);
		save_file(fname, notification);
		add_audit(id, "changed notification");
	}

	unlock_system();
}


/* handle all submitted movement requests and changes ot the directory
   notification */
static void do_dir_changes(void)
{
	int i, j;
	char *v, *dest, *src, *p;
	char dest2[100];
	char src2[100];

	if (guest) {
		not_as_guest("do_dir_changes");
		return;
	}

	lock_system();

	if ((p = cgi_variable("dir_notification")) && 
	    strcmp(dir_notification(directory), p)) {
		/* change to directory notification */
		save_file(".notify", p);
		add_audit(SYSTEMFILE, "changed notification on %s", directory);
	}

	for (i=0;(dest = cgi_vnum(i, &v)); i++) {
		struct message_info info = zero_info;
		if (strcmp(dest,directory) == 0) continue;
		if (strncmp(v, "moveto.", 7)) continue;
		src = v+7;

		if (!valid_id(src) || !get_info(src,&info,max_message_size)) 
			continue;

		add_audit(src, "moved from %s to %s", directory, dest);
		notify_dir(dest, src, "%s moved %s%s from %s to %s", 
			   user, lp_pr_identifier() ,src, directory, dest);
		notify_dir(directory, src, "%s moved %s%s from %s to %s", 
			   user, lp_pr_identifier(), src, directory, dest);
		notify_msg(src, "%s moved %s%s from %s to %s", 
			   user, lp_pr_identifier(), src, directory, dest);

		check_overflow(sizeof(dest)+sizeof(src)+30, sizeof(dest2));

		sprintf(dest2, "../%s/%s", dest, src);
		rename(src, dest2);

		sprintf(dest2, "../%s/%s.notes", dest, src);
		sprintf(src2, "%s.notes", src);
		rename(src2, dest2);

		sprintf(dest2, "../%s/%s.audit", dest, src);
		sprintf(src2, "%s.audit", src);
		rename(src2, dest2);

		sprintf(dest2, "../%s/%s.private", dest, src);
		sprintf(src2, "%s.private", src);
		rename(src2, dest2);

		sprintf(dest2, "../%s/%s.notify", dest, src);
		sprintf(src2, "%s.notify", src);
		rename(src2, dest2);

		for (j=1;j<MAX_REPLIES;j++) {
			sprintf(dest2, "../%s/%s.reply.%d", dest, src, j);
			sprintf(src2, "%s.reply.%d", src, j);
			if (rename(src2, dest2)) break;
		}

		for (j=1;j<MAX_REPLIES;j++) {
			sprintf(dest2, "../%s/%s.followup.%d", dest, src, j);
			sprintf(src2, "%s.followup.%d", src, j);
			if (rename(src2, dest2)) break;
		}
		
	}

	unlock_system();
}


static void load_users(void)
{
	char *p;
	FILE *f;

	if (guest) {
		static char *list[] = {"guest", NULL};
		user_list = list;
		return;
	}

	user_list = load_file_list("users",valid_user);

	if (cgi_variable("createuser") &&
	    (p=cgi_variable("newuser")) && valid_user(p)) {
		if (in_list(user_list, p)) return;

		f = fopen("users", "a");
		if (f) {
			fprintf(f,"%s\n", p);
			fclose(f);
			add_audit(SYSTEMFILE,"created user %s", p);
			user_list = load_file_list("users",valid_user);
		} else {
			fatal("unable to open \"users\" for append\n");
		}
	}
}


static void set_buffering(void)
{
	char *buf = (char *)malloc(BUFSIZ + 1024);
	if (buf) {
		setbuf(stdout, buf);
	}
}

int main(int argc, char *argv[])
{
	char *s;

	load_config(argv[0]);

	/* we must do this before reading anything from the client! */
	do_chroot();

	/* notes files etc need to be available to guest users */
	umask(022);

	set_buffering();

	cgi_load_variables(NULL, NULL);

	load_globals();

	load_users(); 

	load_preferences();

#ifndef __OS2__
	/* make sure that we die if we start looping for some
	   reason - max 5 minute run */
	alarm(300);
#endif


#ifdef DEBUG_COMMENTS
	dump_globals();
#endif

	if (cgi_variable("enter") || !cgi_variable("user")) {
		add_audit(SYSTEMFILE, "connected from %s (%s)",
			  getenv_null("REMOTE_HOST"), getenv_null("REMOTE_ADDR"));
	}

#if 0
	if (user && strcmp(user,"tridge") == 0) {
		mallopt(M_DEBUG, 1);
	}
#endif

	/* if they asked for a new directory then create it now */
	if (!guest && 
	    cgi_variable("createdir") && 
	    (s=cgi_variable("newdir")) && *s) {
		trim_string(s," "," ");
                if (
#ifdef __OS2__
                    mkdir(s)
#else
                    mkdir(s, 0755)
#endif
                         == 0) {
			add_audit(SYSTEMFILE, "created directory %s",s);
		} else {
			fatal("error creating directory");
		}
	}

	main_dir_list = load_dir_list(".",is_directory);
	if (!main_dir_list) fatal("failed to load dir");

	if (cgi_variable("commit") && *user && !guest) {
		save_preferences();
	}

	if (cgi_variable("fullmessage")) {
		max_message_size = 0;
	}

	if (!valid_user(user)) {
		print_title("%s - login",lp_title());
		login_page();
	} else if (cgi_variable("download")) {
		file_download(cgi_variable("download"));
		return 0;
	} else if (cgi_variable("sysaudit")) {
		print_title("%s - system audit trail",lp_title());
		view_system_audit();
	} else if ((!guest && !*fullname) || cgi_variable("preferences")) {
		print_title("%s - user preferences", lp_title());
		edit_preferences();
	} else if (cgi_variable("findid")) {
		find_id();
	} else if (directory && *directory) {
		if (cgi_variable("send") && cgi_variable("id")) {
			print_title("%s - %s/%s", lp_title(),
				    directory, cgi_variable("id"));
			if (chdir(directory)) fatal("can't change directory");
			send_reply();
			view_message(cgi_variable("id"));
		} else if (cgi_variable("compose")) {
			print_title("%s - reply %s/%s", lp_title(),
				    directory, cgi_variable("compose"));
			if (chdir(directory)) fatal("can't change directory");
			compose_reply();
		} else if (cgi_variable("msg_change")) {
			print_title("%s - %s",lp_title(), directory);
			if (chdir(directory)) fatal("can't change directory");
			do_msg_changes();
			do_dir_changes();
			display_dir();
		} else if (cgi_variable("dir_change")) {
			print_title("%s - %s", lp_title(), directory);
			if (chdir(directory)) fatal("can't change directory");
			do_dir_changes();
			display_dir();		
		} else if (cgi_variable("audit")) {
			print_title("%s - audit trail %s/%s", lp_title(), 
				    directory, cgi_variable("auditid"));
			if (chdir(directory)) fatal("can't change directory");
			view_audit();
		} else if (lp_autopatch() && cgi_variable("autopatch")) {
			print_title("%s - autopatch %s/%s", lp_title(), 
				    directory, cgi_variable("auditid"));
			if (chdir(directory)) fatal("can't change directory");
			view_autopatch();
		} else if (lp_decoder() && cgi_variable("decodeview")) {
			print_title("%s - decoder %s/%s", lp_title(), 
				    directory, cgi_variable("decodeview"));
			if (chdir(directory)) fatal("can't change directory");
			file_decodeview();
		} else if (cgi_variable("resend")) {
			print_title("%s - %s", lp_title(), directory);
			if (chdir(directory)) fatal("can't change directory");
			resend_reply();
			display_dir();
		} else if (cgi_variable("id")) {
			print_title("%s - %s/%s", lp_title(),
				    directory, cgi_variable("id"));
			if (chdir(directory)) fatal("can't change directory");
			view_message(cgi_variable("id"));
#if MESSAGE_LINKS
		} else if (cgi_variable("updatelinks")) {
			print_title("%s - links for %s/%s", lp_title(),
				    directory, cgi_variable("links"));
			if (chdir(directory)) fatal("can't change directory");
			do_links();
			display_links(cgi_variable("links"));
		} else if (cgi_variable("links")) {
			print_title("%s - links for %s/%s", lp_title(),
				    directory, cgi_variable("links"));
			if (chdir(directory)) fatal("can't change directory");
			display_links(cgi_variable("links"));
#endif
		} else {
			print_title("%s - %s", lp_title(), directory);
			if (chdir(directory)) fatal("can't change directory");
			display_dir();		
		}
	} else if (cgi_variable("newbug")) {
		print_title("%s - bug submission", lp_title());

		compose_bug_report();
	} else {
		print_title("%s - main page", lp_title());

		if (cgi_variable("sendbugreport")) {
		  send_bug_report();
		}
		main_page();
	}

	print_footer();

	/* just in case we left it locked */
	unlock_system();

	free_list(main_dir_list);

	fflush(stdout);

	return 0;
}
