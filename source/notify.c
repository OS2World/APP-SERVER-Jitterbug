/* 
   The Jitterbug report tracking system
   routines to handle notification of message changes

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

/* NOTE: This module contains routines that are called from both within
   the chrooted environment and outside it. Be careful! */

/*
 * Modified for OS/2 by harald.kipp@egnite.de.
 * All these additional modifications are public domain.
 *
 * $Log: notify.c,v $
 * Revision 1.2  2000/04/25 19:52:10  harald
 * First OS/2 release
 *
 */
#include "jitterbug.h"

/* clean a notify list - removing the current user if listed. This
   prevents users getting email notification of their own actions */
static char *clean_notify(char *list)
{
	extern char *user;
	char *list2, *tok;

	if (!list || !*list || !user || !*user) return list;

	list2 = strdup(list);
	*list2 = 0;

	for (tok=strtok(list," ,\t"); tok; tok=strtok(NULL," ,\t")) {
		if (strcasecmp(user, tok) == 0) continue;
		if (*list2) strcat(list2,",");
		strcat(list2, tok);
	}	

	return list2;
}

/* return the current .notify contents for a directory or "" if none 
   called from within the cgi program
*/
char *dir_notification(char *directory)
{
	char fname[1000];
	char *ret;

	check_overflow(strlen(root_directory()) + strlen(directory) + 20, sizeof(fname));
	sprintf(fname, "%s/%s/.notify", root_directory(), directory);
	ret = load_file(fname, NULL, 0);
	if (!ret) ret = "";
	return ret;
}


/* return the current .notify contents for a message or "" if none 
   must be called with the current working directory set to the directory
   of the message
*/
char *msg_notification(char *id)
{
	char fname[1000];
	char *ret;

	check_overflow(strlen(id) + 10, sizeof(fname));
	sprintf(fname, "%s.notify", id);
	ret = load_file(fname, NULL, 0);
	if (!ret) ret = "";
	return ret;
}


/* give a summary of the message */
static void notify_summary(int fd, char *id)
{
	struct message_info info;
	char *notes;

	memset(&info, 0, sizeof(info));

	if (!get_info(id, &info, 8192)) return;

	smtp_write(fd, "\nMessage summary for %s%s\n", lp_pr_identifier(), id);
	if (info.from)
		smtp_write(fd,"\tFrom: %s\n", unquote(info.from));
	if (info.subject)
		smtp_write(fd,"\tSubject: %s\n", unquote(info.subject));
	if (info.date)
		smtp_write(fd,"\tDate: %s\n", unquote(info.date));
	smtp_write(fd,"\t%d replies \t%d followups\n", 
		   count_replies(id,NULL),
		   count_followups(id,NULL));

	if (lp_base_url()) {
		smtp_write(fd,"\tURL: %s?findid=%s\n", lp_base_url(), id);
	}

	notes = load_ext(id, "notes");
	if (notes)
		smtp_write(fd,"\tNotes: %s\n\n", unquote(notes));

	smtp_write(fd,"\n====> ORIGINAL MESSAGE FOLLOWS <====\n\n");

	smtp_write_data(fd, info.raw);

	if (strlen(info.raw) < info.size) {
		smtp_write(fd,"\n====> MESSAGE TRUNCATED AT %d <====\n\n",
			   strlen(info.raw));
	}
	
}

/* notify the notification list about changes to a directory */
void notify_dir(char *directory,char *id, char *fmt, ...)
{
	va_list ap;
	char buf[1000];
	int fd;
	char *notification = dir_notification(directory);

	notification = clean_notify(notification);

	if (! *notification) {
		/* no notification list */
		return;
	}

	check_overflow(strlen(directory)+strlen(id)+30, sizeof(buf));
	sprintf(buf,"Notification: %s/%s", directory, id);

	fd = smtp_start_mail(lp_from_address(), notification, NULL, NULL, 
			     buf, 5000);
	if (fd == -1) return;

	smtp_write(fd,"\n%s notification\n\n", lp_title());
	va_start(ap, fmt);
	vsprintf(buf,fmt,ap);
	va_end(ap);
	smtp_write_data(fd, buf);

	notify_summary(fd, id);

	smtp_end_mail(fd);
}

/* notify the notification list about changes to a message 
   This must be called with the current working directory
   set to the directory containing the message
*/
void notify_msg(char *id,char *fmt, ...)
{
	va_list ap;
	char buf[1000];
	int fd;
	char *notification = msg_notification(id);

	notification = clean_notify(notification);

	if (! *notification) {
		/* no notification list */
		return;
	}

	check_overflow(strlen(id) + 30, sizeof(buf));
	sprintf(buf,"Notification: %s%s", lp_pr_identifier(), id);

	fd = smtp_start_mail(lp_from_address(), notification, NULL, NULL, 
			     buf, 5000);
	if (fd == -1) return;

	smtp_write(fd,"X-Notification: %s\n", lp_from_address());

	smtp_write(fd,"\n%s notification\n\n", lp_title());
	va_start(ap, fmt);
	vsprintf(buf,fmt,ap);
	va_end(ap);
	smtp_write_data(fd, buf);

	notify_summary(fd, id);

	smtp_end_mail(fd);
}
