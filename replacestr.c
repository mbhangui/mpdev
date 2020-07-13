/*
 * $Log: replacestr.c,v $
 * Revision 1.2  2020-07-13 21:31:14+05:30  Cprogrammer
 * bugfix - copy remaining data after replacement
 *
 * Revision 1.1  2019-04-14 18:30:01+05:30  Cprogrammer
 * Initial revision
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_QMAIL
#include <stralloc.h>
#include <strerr.h>
#include <str.h>
#endif

#ifndef	lint
static char     sccsid[] = "$Id: replacestr.c,v 1.2 2020-07-13 21:31:14+05:30 Cprogrammer Exp mbhangui $";
#endif

/*
 * returns 0 if search string ch not found
 * and leaves buf untouched
 */
int
replacestr(char *str, char *ch, char *rch, stralloc *buf)
{
	int             chlen, rchlen, prev;
	register int    i, cnt;
	register char  *ptr, *s;

	chlen = str_len(ch);
	rchlen = str_len(rch);
	for (ptr = str, prev = cnt = 0; *ptr;) {
		if (!(s = str_str(ptr, ch)))
			break;
		if (!cnt)
			buf->len = 0;
		i = s - str;
		if (!stralloc_catb(buf, ptr, i - prev))
			return (-1);
		if (!stralloc_catb(buf, rch, rchlen))
			return (-1);
		ptr = str + i + chlen; /*- move past the found character */
		prev = i + chlen;
		cnt++;
	}
	if (cnt) {
		/*- copy remaining data */
		if (!stralloc_cats(buf, ptr))
			return (-1);
		if (!stralloc_0(buf))
			return (-1);
		buf->len--;
	}
	return (cnt);
}
