/*
 * $Log: replacestr.h,v $
 * Revision 1.1  2019-04-13 23:39:27+05:30  Cprogrammer
 * replacestr.h
 *
 */
#ifndef REPLACESTR_H
#define REPLACESTR_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_QMAIL
#include <stralloc.h>
#endif

int             replacestr(char *, char *, char *, stralloc *);

#endif
