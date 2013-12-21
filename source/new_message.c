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

*/

/*
  process a new piece of mail coming into the system. The mail is on stdin.
  The default directory to place the mail is given as the 2nd argument to the
  program. If this is not specified then "incoming" is assumed.
 */
/*
 * Modified for OS/2 by harald.kipp@egnite.de.
 * All these additional modifications are public domain.
 *
 * $Log: new_message.c,v $
 * Revision 1.2  2000/04/25 19:52:09  harald
 * First OS/2 release
 *
 */

#include "jitterbug.h"

int done_chroot = 0;
int guest = 0;
char *user="";

void fatal(char *why, ...)
{
	va_list ap;  

	va_start(ap, why);
	vfprintf(stderr, why, ap);
	va_end(ap);
	exit(1);
}

/* get the message subject of a email if possible */
static char *getsubject(char *mbuf)
{
	char *p1, *p2;

	mbuf = strdup(mbuf);
	if (!mbuf) return NULL;

	p1 = strstr(mbuf, "\nSubject:");

	if (p1) {
		p1 += 9;
		p2 = strchr(p1, '\n');
		if (p2) *p2 = 0;
		p1 = strdup(p1);
		trim_string(p1, " ", " ");
	}

	free(mbuf);

	return p1;
}


/* get the message id of a email if possible */
static int getid(char *mbuf)
{
	char *p1, *p2, *p3;
	int lpr;

	mbuf = strdup(mbuf);
	if (!mbuf) return 0;

	lpr = strlen(lp_pr_identifier());

	p1 = mbuf;
	while ((p1 = strstr(p1, "\nSubject:"))) {
		p1 += 9;
		p2 = strchr(p1, '\n');
		if (!p2) return 0;
		*p2++ = 0;
		p3 = p1;
		while ((p3 = strstr(p3,lp_pr_identifier()))) {
			if (atoi(p3+lpr)) return atoi(p3+lpr);
			p3 += lpr;
		}
		p1 = p2;
	}

	free(mbuf);
	return 0;
}

static void autoreply(char *mbuf, int id)
{
	char *subj, *from;
	char *p;
	int fd;
	struct stat st;

	if (!file_exists(AUTOREPLY, &st)) return;

	subj = getsubject(mbuf);
	if (!subj) return;

	if (strncasecmp(subj, "Re:", 3) == 0) return;

	p = strstr(mbuf,"X-Loop: ");
	if (p) {
		from = lp_from_address();
		p += 8;
		if (strncmp(p, from, strlen(from)) == 0) return;
	}

	/* it isn't a loop or a reply - send the autoreply! */
	from = getmailheader(mbuf, "Reply-To:", 0);
	if (!from) 
		from = getmailheader(mbuf, "From:", 0);
	if (!from) 
		from = getmailheader(mbuf, "Return-Path:", 0);

	if (!from) return;

	from = extract_address(from);

	p = (char *)malloc(strlen(subj)+100);
	if (!p) fatal("out of memory");
	sprintf(p,"Re: %s (%s%d)", subj, lp_pr_identifier(), id);

	fd = smtp_start_mail(lp_from_address(), from, NULL, NULL, 
			     p, st.st_size + 1024);

	if (fd == -1) return;

	smtp_write(fd, "X-Loop: %s\n", lp_from_address());
	smtp_write(fd, "\n");

	mbuf = load_file(AUTOREPLY, &st, 0);
	
	/* substitute in the id if wanted */
	p = strstr(mbuf,"%PRNUM%");
	if (p) {
		memset(p,' ',7);
		sprintf(p,"%d", id);
		p[strlen(p)] = ' ';
	}
	if (mbuf) {
		smtp_write_data(fd, mbuf);
	}
	smtp_end_mail(fd);
}


static void forward_mail(char *mbuf, char *to, int id)
{
	char *subj, *from;
	char *p;
	int fd;

	subj = getsubject(mbuf);
	if (!subj) return;

	from = getmailheader(mbuf, "Reply-To:", 0);
	if (!from) 
		from = getmailheader(mbuf, "From:", 0);
	if (!from) 
		from = getmailheader(mbuf, "Return-Path:", 0);

	if (!from) return;

	from = extract_address(from);

	if (strncasecmp(subj, "Re:", 3) == 0 &&
	    match_string(from, lp_reply_strings())) return;

	if (strstr(subj, lp_pr_identifier())) {
		p = subj;
	} else {
		p = (char *)malloc(strlen(subj)+100);
		if (!p) fatal("out of memory");
		sprintf(p,"%s (%s%d)", subj, lp_pr_identifier(), id);
	}

	fd = smtp_start_mail(from, to, NULL, NULL, 
			     p, strlen(mbuf) + 1024);

	if (fd == -1) return;

	smtp_write(fd, "CC: %s\n", lp_from_address());
	smtp_write(fd, "X-Loop: %s\n", lp_from_address());
	smtp_write(fd, "\n");

	p = strstr(mbuf,"\n\n");
	if (p) p += 2;
	if (!p) {
		p = strstr(mbuf,"\r\n\r\n");
		if (p) p += 4;
	}
	if (!p) p = mbuf;

	smtp_write_data(fd, p);

	smtp_end_mail(fd);
}
	

int process_mail(char *def_dir)
{
	FILE *f = fopen(".newmsg","w");
	char buf[100], fname[1000];
	char *mbuf;
	int id, followup=0, nextid=0;
	char idname[20];
	char *p, *p2, *dir, *basename=NULL;
	char *from, *subject;

        /* read the message into a temporary file */
	while (!feof(stdin)) {
		int len = fread(buf, 1, sizeof(buf), stdin);
		if (len > 0)
			fwrite(buf, 1, len, f);
	}
	fclose(f);

	fname[0] = 0;

	mbuf = load_file(".newmsg", NULL, 0);
	if (!mbuf) {
		fprintf(stderr,"Can't read .newmsg\n");
		return 1;
	}

	from = getmailheader(mbuf, "Reply-To:", 0);
	if (!from) 
		from = getmailheader(mbuf, "From:", 0);
	if (!from) 
		from = getmailheader(mbuf, "Return-Path:", 0);

	/* don't process corrupt mail */
	if (!from) {
		fprintf(stderr,"Corrupt mail headers\n");
		return 1;
	}

	from = extract_address(from);
	
	/* never process mail from ourselves! */
	if (strcmp(from, lp_from_address()) == 0) {
		fprintf(stderr,"Mail loop - discarding mail\n");
		return 1;
	}


	/* work out if it has an existing id */
	id = getid(mbuf);
	sprintf(idname,"%d", id);

	if (id != 0) {
		basename = find_file(".", idname);
		if (basename) {
			if (match_string(from, lp_reply_strings())) {
				followup = count_replies(basename, NULL)+1;
				check_overflow(strlen(basename)+20, sizeof(fname));
				sprintf(fname,"%s.reply.%d", basename, followup);
			} else {
				followup = count_followups(basename, NULL)+1;
				check_overflow(strlen(basename)+20, sizeof(fname));
				sprintf(fname,"%s.followup.%d", basename, followup);
			}
		}
	}

	if (! *fname) {
		char *idfile = load_file(".nextid", NULL, 0);
		nextid=1;
		if (idfile) nextid = atoi(idfile);
		sprintf(idname,"%d", nextid+1);
		save_file(".nextid", idname);
		sprintf(idname,"%d", nextid);
		check_overflow(strlen(def_dir)+20, sizeof(fname));
		sprintf(fname,"%s/%d", def_dir, nextid);
	}

	p = strrchr(fname,'/');
	if (!p) {
		fprintf(stderr,"invalid filename\n");
		exit(1);
	}
	*p = 0;
	dir = strdup(fname);
	*p = '/';

        if (!is_directory(dir)) {
#ifdef __OS2__
                mkdir(dir);
#else
                mkdir(dir,0755);
#endif
	}

	if (rename(".newmsg", fname)) {
		fprintf(stderr,"rename .newmsg %s failed\n", fname);
	}

	if (lp_forward_all()) {
		forward_mail(mbuf, lp_forward_all(), id?id:nextid);
	}

	/* forward to "forward public" if message not marked private */
	subject = getmailheader(mbuf, "Subject:", 0);

	if (subject &&
	    lp_forward_public() && !strstr(subject,"PRIVATE")) {
		forward_mail(mbuf, lp_forward_public(), id?id:nextid);
	}

	if (!id && strcmp(def_dir, lp_incoming())==0) autoreply(mbuf, nextid);
	
        chdir(dir);

	/* make sure we don't get a notification loop */
	p2 = "X-Notification: ";
	p = strstr(mbuf,p2);
	if (p && strncmp(p+strlen(p2), lp_from_address(), strlen(lp_from_address()))==0) {
		return 0;
	}

	notify_dir(dir, idname, "new message %s\n", fname);

	if (followup) {
		notify_msg(idname,"new followup %d\n", followup);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	char *dir;

        if (argc <= 1) {
		fprintf(stderr,"You must specify a configuration to use\n");
		exit(1);
	}

	load_config(argv[1]);

	if (chdir(root_directory())) {
		fprintf(stderr,"Failed to chdir to %s : %s\n", 
			root_directory(), strerror(errno));
		exit(1);
        }

	if (argc > 2)
		dir = argv[2];
	else
		dir = lp_incoming();

	umask(022);

	lock_system();
	ret = process_mail(dir);
	unlock_system();

	return ret;
}
