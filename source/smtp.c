/* 
   The JitterBug bug tracking system
   smtp utility functions

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
 * $Log: smtp.c,v $
 * Revision 1.2  2000/04/25 19:52:10  harald
 * First OS/2 release
 *
 */

#include "jitterbug.h"

#define SMTP_PORT 25

static char smtp_in_buf[1024];
static char smtp_out_buf[8192];

char *smtp_error(void)
{
	char *ret;

	ret = (char *)malloc(strlen(smtp_in_buf) + strlen(smtp_out_buf) + 100);
	if (!ret) return "SMTP error malloc failed";

	sprintf(ret, "After command: %s\nReceived: %s\n",
		smtp_out_buf, smtp_in_buf);
	return quotedup(ret);
}

static int ismtp_open(void)
{
	char *dest = lp_smtp_address();
	struct in_addr addr;
	int type = SOCK_STREAM;
	struct sockaddr_in sock_out;
	int res;

        /* create a socket to write to */
#ifdef __OS2__
        res = socket(AF_INET, type, 0);
#else
        res = socket(PF_INET, type, 0);
#endif
	if (res == -1) {
		return -1;
	}

	addr.s_addr = inet_addr(dest);
  
	sock_out.sin_addr = addr;
	sock_out.sin_port = htons(SMTP_PORT);
#ifdef __OS2__
        sock_out.sin_family = AF_INET;
#else
        sock_out.sin_family = PF_INET;
#endif

	if (connect(res,(struct sockaddr *)&sock_out,sizeof(sock_out))) {
		close(res);
		return -1;
	}

	return res;
}


static int ismtp_write_data(int fd, char *p)
{
	int eom = 0;
	if (strcmp(p,"\n.\n") == 0) {
		eom = 1;
	}

	while (*p) {
		if (p[0] == '\r') {
			p++; continue;
		}
                if (p[0] == '\n') {
#ifdef __OS2__
                        if (send(fd,"\r",1,0) != 1) return 0;
#else
                        if (write(fd,"\r",1) != 1) return 0;
#endif
		}
#ifdef __OS2__
                if (send(fd,p,1,0) != 1) return 0;
#else
                if (write(fd,p,1) != 1) return 0;
#endif
                if (!eom && p[0] == '\n' && p[1] == '.') {
#ifdef __OS2__
                        if (send(fd,".",1,0) != 1) return 0;
#else
                        if (write(fd,".",1) != 1) return 0;
#endif
                }
		p++;
	}
	return 1;
}

/* wait for a SMTP success code. If we receive a type 4 or 5 error
   code then return a failure */
static int ismtp_waitfor(int fd, char code)
{
	char c;
	int offset=0;
	char *line=smtp_in_buf;

	memset(smtp_in_buf, 0, sizeof(smtp_in_buf));

#ifdef __OS2__
        while (recv(fd,&c,1,0) == 1) {
#else
        while (read(fd,&c,1) == 1) {
#endif
		if (offset < sizeof(smtp_in_buf))
			smtp_in_buf[offset++] = c;
		if (c != '\n') continue;

		/* we have a complete line */
		if (isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2]) &&
		    line[3] == ' ') {
			/* we have a SMTP return code */
			if (line[0] == code) {
				memset(smtp_in_buf, 0, sizeof(smtp_in_buf));
				memset(smtp_out_buf, 0, sizeof(smtp_out_buf));
				return 1;
			}
			
			/* we got an unexpected SMTP error code */
			return 0;
		}

		line = &smtp_in_buf[offset];
	}

	return 0;
}

static int ismtp_start_mail(char *from, char *to, char *cc, char *bcc, char *subject, int len)
{
	char *p, *tok;
	int fd = ismtp_open();
	if (fd == -1) {
                return -1;
	}

	if (!ismtp_waitfor(fd, '2')) {
		close(fd);
                return -1;
	}

        smtp_write(fd,"EHLO localhost\n");
	if (!ismtp_waitfor(fd, '2')) {
		/* try again without esmtp */
		smtp_write(fd,"HELO localhost\n");
		
		if (!ismtp_waitfor(fd, '2')) {
			close(fd);
			return -1;
		}
	}

	if (strchr(from,'\n') || strchr(from,'\r')) {
		close(fd);
		return -1;
	}

	smtp_write(fd,"MAIL FROM: <%s> SIZE=%d\n", 
		   extract_address(from),len+5000);

	if (!ismtp_waitfor(fd, '2')) {
		/* try again without the SIZE= option */
		smtp_write(fd,"MAIL FROM: <%s>\n", 
			   extract_address(from));
		if (!ismtp_waitfor(fd, '2')) {
			close(fd);
			return -1;
		}
	}
	
	p = strdup(to);
	for (tok=strtok(p," ,\t"); tok; tok=strtok(NULL," ,\t")) {
		smtp_write(fd,"RCPT TO: <%s>\n", user_address(tok));
		if (!ismtp_waitfor(fd, '2')) {
			close(fd);
			return -1;
		}
	}
	free(p);
	if (cc && *cc) {
		p = strdup(cc);
		for (tok=strtok(p," ,\t"); tok; tok=strtok(NULL," ,\t")) {
			smtp_write(fd,"RCPT TO: <%s>\n", user_address(tok));
			if (!ismtp_waitfor(fd, '2')) {
				close(fd);
				return -1;
			}
		}
		free(p);
	}
	if (bcc && *bcc) {
		p = strdup(bcc);
		for (tok=strtok(p," ,\t"); tok; tok=strtok(NULL," ,\t")) {
			smtp_write(fd,"RCPT TO: <%s>\n", user_address(tok));
			if (!ismtp_waitfor(fd, '2')) {
				close(fd);
				return -1;
			}
		}
		free(p);
	}
	smtp_write(fd,"DATA\n");
	if (!ismtp_waitfor(fd, '3')) {
		close(fd);
		return -1;
	}
	if (subject) {
		smtp_write(fd,"From: %s\n", from);
		smtp_write(fd,"To: %s\n", to);
		smtp_write(fd,"Subject: %s\n", subject);
		if (cc && *cc)
			smtp_write(fd,"CC: %s\n", cc);
	}

	return fd;
}

static int ismtp_end_mail(int fd)
{
	smtp_write(fd,"\n.\n");
	if (!ismtp_waitfor(fd, '2')) {
		close(fd);
		return -1;
	}
	smtp_write(fd,"QUIT\n");
	if (!ismtp_waitfor(fd, '2')) {
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

static FILE *mailer_f;

static int msmtp_open(void)
{
	mailer_f = popen(lp_mailer(), "w");
	if (!mailer_f) return -1;

	return fileno(mailer_f);
}


static int msmtp_write_data(int fd, char *buf)
{
	char *p = buf;

	while (*p) {
		if (p[0] == '\r') {
			p++; continue;
		}
		if (write(fd,p,1) != 1) return 0;
		p++;
	}
	return 1;
}

static int msmtp_start_mail(char *from, char *to, char *cc, char *bcc, char *subject, int len)
{
	int fd = msmtp_open();

	if (fd == -1) {
		return -1;
	}

	if (subject) {
		time_t t = time(NULL);

		smtp_write(fd,"From: %s\n", from);
		smtp_write(fd,"To: %s\n", to);
		if (cc && *cc)
			smtp_write(fd,"CC: %s\n", cc);
		if (bcc && *bcc)
			smtp_write(fd,"BCC: %s\n", cc);
		smtp_write(fd,"Subject: %s\n", subject);
		smtp_write(fd,"Date: %s", ctime(&t));
	}

	return fd;
}

static int msmtp_end_mail(int fd)
{
	pclose(mailer_f);
	return 0;
}


static int use_mailer(void)
{
	char *m = lp_mailer();
	if (m && *m) return 1;
	return 0;
}

int smtp_write(int fd, char *fmt, ...)
{
	va_list ap;  

	va_start(ap, fmt);
	vslprintf(smtp_out_buf, sizeof(smtp_out_buf), fmt, ap);
	va_end(ap);

	if (use_mailer()) 
		return msmtp_write_data(fd, smtp_out_buf);
	else
		return ismtp_write_data(fd, smtp_out_buf);
}

void smtp_write_data(int fd, char *p)
{
	if (use_mailer()) 
		msmtp_write_data(fd, p);
	else
		ismtp_write_data(fd, p);
}


int smtp_start_mail(char *from, char *to, char *cc, char *bcc, char *subject, int len)
{
        if (use_mailer())
		return msmtp_start_mail(from, to, cc, bcc, subject, len);
	else
		return ismtp_start_mail(from, to, cc, bcc, subject, len);
}

int smtp_end_mail(int fd)
{
	if (use_mailer()) 
		return msmtp_end_mail(fd);
	else
		return ismtp_end_mail(fd);
}
