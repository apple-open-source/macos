/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifdef __APPLE_PR3375657_HACK__

/*
 * When mutexes or spinlocks were added for thread safety, the system would
 * hang during the boot process, just after changing to the blue background.
 * So for the common case of not calling _s[eh]m_hack_{add,init}(), we just
 * use static name lists.  This should be reinvestigated when there is time.
 */

#define PRIVATE		__private_extern__
//#define SEM_DEBUG_FILE	"/tmp/sem_names"
//#define SHM_DEBUG_FILE	"/tmp/shm_names"

#if defined(SEM_DEBUG_FILE) || defined(SHM_DEBUG_FILE)
#include <stdio.h>
#include <unistd.h>
#endif /* defined(SEM_DEBUG_FILE) || defined(SHM_DEBUG_FILE) */
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#ifdef SEM_DEBUG_FILE
#define SEM_PRINTF(fmt, args...) \
{ \
	FILE *_sem_fp_; \
	if (access(SEM_DEBUG_FILE, F_OK) == 0 && \
	    (_sem_fp_ = fopen(SEM_DEBUG_FILE, "a")) != NULL) { \
		fprintf(_sem_fp_, fmt, ## args); \
		fclose(_sem_fp_); \
	} \
}
#endif /* SEM_DEBUG_FILE */
#ifdef SHM_DEBUG_FILE
#define SHM_PRINTF(fmt, args...) \
{ \
	FILE *_shm_fp_; \
	if (access(SHM_DEBUG_FILE, F_OK) == 0 && \
	    (_shm_fp_ = fopen(SHM_DEBUG_FILE, "a")) != NULL) { \
		fprintf(_shm_fp_, fmt, ## args); \
		fclose(_shm_fp_); \
	} \
}
#endif /* SHM_DEBUG_FILE */

/*-----------------------------------------------------------------------
 * For the Hack structure:
 *
 * first >= 0	starting serial number
 * first < 0	no serial number
 * last		ending serial number (only if first >= 0)
 * debug	whether an option 'D' can be appended
 *-----------------------------------------------------------------------*/
typedef struct {
	const char *name;
	int first;
	int last;
	int debug;
} Hack;

/*-----------------------------------------------------------------------
 * For the HackList structure:
 *
 * list		the list of Hack structures
 * cur		the number of valid Hack structures in list
 * max		the actual number of Hack structures allocated
 *-----------------------------------------------------------------------*/
#define	HACKLISTDELTA	16
#define	HACKLISTSTART	16
typedef struct {
	const Hack *list;
	int cur;
	int max;
} HackList;

static const Hack sem_hack_default_names[] = {
	{"EDBPool", 1, 255, 0},
	{"EDDPoolLock", -1, 0, 0},
	{"Mso97SharedDg", 1920, 2047, 1},
	{"Office", 1920, 2047, 1},
	{"PT_EDBPool", 1, 255, 0},
	{"PT_EDDPoolLock", -1, 0, 0},
	{"PT_Mso97SharedDg", 1920, 2047, 0},
	{"PT_Office", 1920, 2047, 0},
	{"ShMemExtCritSection", -1, 0, 0},
	{"ShMemIntCritSection", -1, 0, 0},
	{NULL, 0, 0, 0},
};
static HackList sem_hack_defaults = {
	sem_hack_default_names,
	(sizeof(sem_hack_default_names) / sizeof(const Hack)) - 1,
	0
};
static HackList *sem_hack_names = &sem_hack_defaults;

static const Hack shm_hack_default_names[] = {
	{"EDBPool", 1, 255, 0},
	{"EDDPoolLock", -1, 0, 0},
	{"Mso97SharedDg", 1920, 2047, 1},
	{"Office", 1920, 2047, 1},
	{"PT_EDBPool", 1, 255, 0},
	{"PT_EDDPoolLock", -1, 0, 0},
	{"PT_Mso97SharedDg", 1920, 2047, 0},
	{"PT_Office", 1920, 2047, 0},
	{"PT_ShMemRefCount", -1, 0, 0},	/* not specified by MS, but seen */
	{"ShMemRefCount", -1, 0, 0},
	{NULL, 0, 0, 0},
};
static HackList shm_hack_defaults = {
	shm_hack_default_names,
	(sizeof(shm_hack_default_names) / sizeof(const Hack)) - 1,
	0
};
static HackList *shm_hack_names = &shm_hack_defaults;

static int comparkey(const void *key, const void *hname);
static int comparstr(const void *a, const void *b);
static int dosearch(const char *name, const HackList *hl);
static int hl_add(HackList *hl, const Hack *h);
static void hl_free(HackList *hl);
static HackList *hl_init(void);
static HackList *initList(const Hack *list);
int _sem_hack_add(const Hack *list);
void _sem_hack_init(void);
PRIVATE int _sem_match(const char *name);
int _shm_hack_add(const Hack *list);
void _shm_hack_init(void);
PRIVATE int _shm_match(const char *name);

/*-----------------------------------------------------------------------
 * comparkey - used by bsearch to find the Hack structure with the given key
 *-----------------------------------------------------------------------*/
static int
comparkey(const void *key, const void *h)
{
	return strcmp(key, ((const Hack *)h)->name);
}

/*-----------------------------------------------------------------------
 * comparstr - used by qsort to sort the Hack list
 *-----------------------------------------------------------------------*/
static int
comparstr(const void *a, const void *b)
{
	return strcmp(((const Hack *)a)->name, ((const Hack *)b)->name);
}

/*-----------------------------------------------------------------------
 * dosearch - search of the given name in the given HackList.  First see
 * if there is a trailing D, and a serial number.  If the serial number
 * exists, try to match without the serial number, checking the series
 * range and whether the trailing D is allowed.  Otherwise, try to match
 * the whole string, but check if the matched Hack structure requires a
 * serial number.
 *-----------------------------------------------------------------------*/
static int
dosearch(const char *name, const HackList *hl)
{
	int series;
	int len = strlen(name);
	const char *end, *p;
	char *key;
	const Hack *h;

	end = name + len - 1;
	if (*end != 'D')
		end++;
	p = end - 1;
	while (p >= name && *p >= '0' && *p <= '9')
		p--;
	p++;
	if (p < end && (len = p - name) > 0) {
		key = alloca(len + 1);
		if (key) {
			series = atoi(p);
			strncpy(key, name, len);
			key[len] = 0;
			h = (const Hack *)bsearch(key, hl->list, hl->cur,
			    sizeof(const Hack), comparkey);
			if (h && h->first >= 0
			    && series >= h->first && series <= h->last
			    && (*end == 0 || h->debug))
				return 1;
		}
	}
	h = (const Hack *)bsearch(name, hl->list, hl->cur, sizeof(const Hack),
	    comparkey);
	return (h && h->first < 0);
}

/*-----------------------------------------------------------------------
 * hl_add - append to the given HackList a copy of the given Hack structure
 *-----------------------------------------------------------------------*/
static int
hl_add(HackList *hl, const Hack *c)
{
	int i = hl->cur;
	Hack *h;

	if (!c->name)
		return -1;
	if (i >= hl->max) {
		int s = hl->max + HACKLISTDELTA;
		const Hack *new = (const Hack *)realloc((void *)hl->list,
		    s * sizeof(const Hack));

		if (!new)
			return -1;
		hl->list = new;
		hl->max = s;
	}
	h = (Hack *)(hl->list + i);
	if ((h->name = strdup(c->name)) == NULL)
		return -1;
	h->first = c->first;
	h->last = c->last;
	h->debug = c->debug;
	hl->cur++;
	return 0;
}

/*-----------------------------------------------------------------------
 * hl_free - deallocate all memory from the given HackList
 *-----------------------------------------------------------------------*/
static void
hl_free(HackList *hl)
{
	const Hack *h;
	int i;

	for (h = hl->list, i = hl->cur; i > 0; h++, i--)
		free((void *)h->name);
	free((void *)hl->list);
	free(hl);
}

/*-----------------------------------------------------------------------
 * hl_init - create a new HackList, with preallocated Hack structures
 *-----------------------------------------------------------------------*/
static HackList *
hl_init(void)
{
	HackList *hl = (HackList *)malloc(sizeof(HackList));

	if (!hl)
		return NULL;
	hl->list = (Hack *)malloc(HACKLISTSTART * sizeof(Hack));
	if (!hl->list) {
		free(hl);
		return NULL;
	}
	hl->cur = 0;
	hl->max = HACKLISTSTART;
	return hl;
}

/*-----------------------------------------------------------------------
 * initList - initialize a new HackList with the given list of Hack structures
 *-----------------------------------------------------------------------*/
static HackList *
initList(const Hack *list)
{
	HackList *hl = hl_init();

	if (hl == NULL)
		return NULL;
	for (; list->name; list++)
		if (hl_add(hl, list) < 0) {
			hl_free(hl);
			return NULL;
		}
	return hl;
}

/*-----------------------------------------------------------------------
 * PUBLIC _sem_hack_add - add the given Hack list to sem_hack_names.
 *-----------------------------------------------------------------------*/
int
_sem_hack_add(const Hack *list)
{
	if (list == NULL)
		return -1;
	if (sem_hack_names == &sem_hack_defaults) {
		HackList *hl = initList(sem_hack_default_names);
		if (!hl)
			return -1;
		sem_hack_names = hl;
	}
	for (; list->name; list++)
		if (hl_add(sem_hack_names, list) < 0)
			return -1;
	qsort((void *)sem_hack_names->list, sem_hack_names->cur,
	    sizeof(const Hack), comparstr);
	return 0;
}

/*-----------------------------------------------------------------------
 * PUBLIC _sem_hack_init - reinitialize sem_hack_names to the default
 *-----------------------------------------------------------------------*/
void
_sem_hack_init(void)
{
	if (sem_hack_names == &sem_hack_defaults)
		return;
	hl_free(sem_hack_names);
	sem_hack_names = &sem_hack_defaults;
}

/*-----------------------------------------------------------------------
 * _sem_match - try to match the given named to sem_hack_names.  Called
 * by sem_open() and sem_unlink().
 *-----------------------------------------------------------------------*/
PRIVATE int
_sem_match(const char *name)
{
#ifdef SEM_DEBUG_FILE
	int match;
#endif /* SEM_DEBUG_FILE */

	if (!name || !*name)
		return 0;
#ifdef SEM_DEBUG_FILE
	match = dosearch(name, sem_hack_names);
	if (!match)
		SEM_PRINTF("%s\n", name);
	return match;
#else /* SEM_DEBUG_FILE */
	return dosearch(name, sem_hack_names);
#endif /* SEM_DEBUG_FILE */
}

/*-----------------------------------------------------------------------
 * PUBLIC _shm_hack_add - add the given Hack list to shm_hack_names.
 *-----------------------------------------------------------------------*/
int
_shm_hack_add(const Hack *list)
{
	if (list == NULL)
		return -1;
	if (shm_hack_names == &shm_hack_defaults) {
		HackList *hl = initList(shm_hack_default_names);
		if (!hl)
			return -1;
		shm_hack_names = hl;
	}
	for (; list->name; list++)
		if (hl_add(shm_hack_names, list) < 0)
			return -1;
	qsort((void *)shm_hack_names->list, shm_hack_names->cur,
	    sizeof(const Hack), comparstr);
	return 0;
}

/*-----------------------------------------------------------------------
 * PUBLIC _shm_hack_init - reinitialize shm_hack_names to the default
 *-----------------------------------------------------------------------*/
void
_shm_hack_init(void)
{
	if (shm_hack_names == &shm_hack_defaults)
		return;
	hl_free(shm_hack_names);
	shm_hack_names = &shm_hack_defaults;
}

/*-----------------------------------------------------------------------
 * _shm_match - try to match the given named to shm_hack_names.  Called
 * by shm_open() and shm_unlink().
 *-----------------------------------------------------------------------*/
PRIVATE int
_shm_match(const char *name)
{
#ifdef SHM_DEBUG_FILE
	int match;
#endif /* SHM_DEBUG_FILE */

	if (!name || !*name)
		return 0;
#ifdef SHM_DEBUG_FILE
	match = dosearch(name, shm_hack_names);
	if (!match)
		SHM_PRINTF("%s\n", name);
	return match;
#else /* SHM_DEBUG_FILE */
	return dosearch(name, shm_hack_names);
#endif /* SHM_DEBUG_FILE */
}

#endif /* __APPLE_PR3375657_HACK__ */
