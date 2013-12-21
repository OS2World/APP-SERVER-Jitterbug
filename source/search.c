/*
 * Modified for OS/2 by harald.kipp@egnite.de.
 * All these additional modifications are public domain.
 *
 * $Log: search.c,v $
 * Revision 1.2  2000/04/25 19:52:10  harald
 * First OS/2 release
 *
 */
#include "jitterbug.h"

#if HAVE_REGEX_H
  
/* a simple search function using POSIX.2 regex */
    
int exp_match(char *buf,char *exp)
{
  	static int initialised;
 	static regex_t regex[1];
 	regmatch_t regmatch[1];
	
  	if (!buf || !*buf) return 0;
  	if (!exp || !*exp) return 1;

	if (!initialised) {
		/* this is a hack that relies on the fact that JitterBug
		   currently only searches for one expression in the life
		   of the process */
	        regcomp(regex, exp, REG_NEWLINE);
		initialised = 1;
	}
	return 0 == regexec(regex, buf, 1, regmatch, 0); 
}

#else
/* a simple search function using regexp */

#define INIT       char *sp = exp_ptr;
#define GETC()       (*sp++)
#define PEEKC()      (*sp)
#define UNGETC(c)  (--sp)
#define RETURN(c) return(NULL);
#define ERROR(c)   return(NULL);

static char *exp_ptr;

#include <regexp.h>

#ifndef ESIZE
#define ESIZE 1024
#endif

int exp_match(char *buf,char *exp)
{
	static char cexp[ESIZE];
	static int initialised;

	if (!buf || !*buf) return 0;
	if (!exp || !*exp) return 1;

	if (!initialised) {
		/* this is a hack that relies on the fact that JitterBug
		   currently only searches for one expression in the life
		   of the process */
		exp_ptr = exp;
		compile(exp, cexp, &cexp[ESIZE], '\0');
		initialised = 1;
	}

	return step(buf, cexp);
}
#endif


int external_search(char *name, char *sexp)
{
	char *prog = lp_search_program();
	static char **results;
	static int len;
	static int alloc_len;
	int i;
	char buf[1024];
	FILE *f;
	char *p;

	p = strrchr(name,'/');
	if (p) name = p+1;

	if (results) {
		for (i=0;i<len;i++)
			if (strcmp(results[i], name)==0) return 1;
		return 0;
	}

	p = sexp;

	while (*p) {
		if (!isalnum(*p) && !strchr(";_|=+ &^#@!(){}[].",*p))
			fatal("invalid character in expression");
		p++;
	}

	alloc_len += 1024;
	results = (char **)malloc(sizeof(results[0])*alloc_len);
	if (!results) fatal("out of memory");

	check_overflow(strlen(prog) + strlen(sexp) + 10, sizeof(buf));

	sprintf(buf, "%s '%s'", prog, sexp);

	f = popen(buf,"r");

	if (!f) {
		fatal("launch of %s failed", prog);
	}

	while (fgets(buf, sizeof(buf)-1, f)) {
		if (buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = 0;

		p = strrchr(buf,'/');
		if (!p) {
			p = buf;
		} else {
			p++;
		}

		if (len == alloc_len) {
			alloc_len += 1024;
			results = (char **)realloc(results, 
						   sizeof(results[0])*alloc_len);
			if (!results) fatal("out of memory");
		}
		results[len] = strdup(p);
		if (!results[len]) fatal("out of memory");
		len++;
	}
	
	pclose(f);

	return external_search(sexp, name);
}
