/* 
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	File: strings.c
 * 	Author: Robert V. Baron, Carnegie Mellon University
 *	Date:	??/92
 *
 *	String functions.
 */

#include <string.h>

#ifdef	strcpy
#undef strcmp
#undef strncmp
#undef strcpy
#undef strncpy
#undef strlen
#endif

/*
 * Abstract:
 *	strcmp (s1, s2) compares the strings "s1" and "s2".
 *	It returns 0 if the strings are identical. It returns
 *	> 0 if the first character that differs in the two strings
 *	is larger in s1 than in s2 or if s1 is longer than s2 and 
 *	the contents are identical up to the length of s2.
 *	It returns < 0 if the first differing character is smaller 
 *	in s1 than in s2 or if s1 is shorter than s2 and the
 *	contents are identical up to the length of s1.
 */

int __attribute__ ((pure))
strcmp(
	const char *s1,
	const char *s2)
{
	unsigned int a, b;

	do {
		a = *s1++;
		b = *s2++;
		if (a != b)
			return a-b;	/* includes case when
					   'a' is zero and 'b' is not zero
					   or vice versa */
	} while (a != '\0');

	return 0;	/* both are zero */
}


/*
 * Abstract:
 *	strncmp (s1, s2, n) compares the strings "s1" and "s2"
 *	in exactly the same way as strcmp does.  Except the
 *	comparison runs for at most "n" characters.
 */

int __attribute__ ((pure))
strncmp(
	const char *s1,
	const char *s2,
	size_t n)
{
	unsigned int a, b;

	while (n != 0) {
		a = *s1++;
		b = *s2++;
		if (a != b)
			return a-b;	/* includes case when
					   'a' is zero and 'b' is not zero
					   or vice versa */
		if (a == '\0')
			return 0;	/* both are zero */
		n--;
	}

	return 0;
}


/*
 * Abstract:
 *	strcpy copies the contents of the string "from" including 
 *	the null terminator to the string "to". A pointer to "to"
 *	is returned.
 */

char *
strcpy(
	char *to,
	const char *from)
{
	char *ret = to;

	while ((*to++ = *from++) != '\0')
		continue;

	return ret;
}

/*
 * Abstract:
 *	strncpy copies "count" characters from the "from" string to
 *	the "to" string. If "from" contains less than "count" characters
 *	"to" will be padded with null characters until exactly "count"
 *	characters have been written. The return value is a pointer
 *	to the "to" string.
 */

char *
strncpy(
	char *to,
	const char *from,
	size_t count)
{
	char *ret = to;

	while (count != 0) {
		count--;
		if ((*to++ = *from++) == '\0')
			break;
	}

	while (count != 0) {
		*to++ = '\0';
		count--;
	}

	return ret;
}

/*
 * Abstract:
 *	strlen returns the number of characters in "string" preceding
 *	the terminating null character.
 */

size_t __attribute__ ((pure))
strlen(
	const char *string)
{
	const char *ret = string;

	while (*string++ != '\0')
		continue;

	return string - 1 - ret;
}

char *strchr(const char *s, int c)
{
	for (; *s; s++)
		if (*s == c)
			return s;
}

/*
 * Abstract:
 *	strsep splits "string" into tokens separated by "delim", by putting a
 *	\0 at the first occurrence of some of the characters of delim, and
 *	advancing the pointer past it. It returns a pointer to the start of the
 *	string.
 */

char *
strsep(
	char **stringp, const char *delim)
{
	char *c, *orig = *stringp;
	if (orig == NULL)
		return NULL;

	for (c = *stringp; *c; c++)
		if (strchr(delim, *c)) {
			*c = 0;
			*stringp = c+1;
			return orig;
		}

	*stringp = NULL;
	return orig;
}

/*
 * Abstract:
 *	strstr returns the first occurrence of "needle" in the "haystack"
 *	string, or NULL if there is none.
 */

char *
strstr(
	const char *haystack, const char *needle)
{
	int n = strlen(needle);

	for (; *haystack; haystack++) {
		if (!strncmp(haystack, needle, n))
			return (char*) haystack;
	}
}

/*
 * Abstract:
 *	memset writes value "c" in the "n" bytes starting at address "s".
 *	The return value is a pointer to the "s" string.
 */

void *
memset(
	void *_s, int c, size_t n)
{
	char *s = _s;
	int i;

	for (i = 0; i < n ; i++)
		s[i] = c;

	return _s;
}

/*
 * Abstract:
 *	memcpy copies "n" bytes starting at address "s" to address "d".
 *	The return value is a pointer to the "d" string.
 */

void *
memcpy(
	void *_d, const void *_s, size_t n)
{
	char *s = _s;
	char *d = _d;
	int i;

	for (i = 0; i < n ; i++)
		d[i] = s[i];

	return _d;
}

/*
 * Abstract:
 *	memcmp compares "n" bytes starting at address "s1" with address "s2"
 *	The return value is negative, nul, or positive if s1 is, respectively,
 *	less than, the same as, or greater than s2.
 */

int
memcmp(
	const void *_s1, const void *_s2, size_t n)
{
	char *s1 = _s1;
	char *s2 = _s2;
	int i;

	for (i = 0; i < n ; i++)
		if (s1[i] != s2[i])
			return s1[i]-s2[i];

	return 0;
}
