/*++
/* NAME
/*      sdbm 3h
/* SUMMARY
/*      SDBM Simple DBM: ndbm work-alike hashed database library
/* SYNOPSIS
/*      include "sdbm.h"
/* DESCRIPTION
/*	This file includes the public domain SDBM (ndbm work-alike hashed
/*	database library), based on Per-Aake Larson's Dynamic Hashing
/*	algorithms. BIT 18 (1978).
/*	author: oz@nexus.yorku.ca
/*	status: public domain
/*	The file has been patched following the advice of Uwe Ohse
/*	<uwe@ohse.de>:
/*	--------------------------------------------------------------
/*	this patch fixes a problem with sdbms .dir file, which arrises when
/*	a second .dir block is needed for the first time. read() returns 0
/*	in that case, and the library forgot to initialize that new block.
/*
/*	A related problem is that the calculation of db->maxbno is wrong.
/*	It just appends 4096*BYTESIZ bits, which is not enough except for
/*	small databases (.dir basically doubles everytime it's too small).
/*	--------------------------------------------------------------
/*	According to Uwe Ohse, the patch has also been submitted to the
/*	author of SDBM. (The 4096*BYTESIZ bits comment may apply with a
/*	different size for Postfix/TLS, as the patch was sent against the
/*	original SDBM distributiona and for Postfix/TLS I have changed the
/*	default sizes.
/* .nf
/*--*/

/*
 * sdbm - ndbm work-alike hashed database library
 * based on Per-Aake Larson's Dynamic Hashing algorithms. BIT 18 (1978).
 * author: oz@nexus.yorku.ca
 * status: public domain.
 *
 * core routines
 */

#include <stdio.h>
#include <stdlib.h>
#ifdef WIN32
#include <io.h>
#include <errno.h>
#else
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#ifdef __STDC__
#include <stddef.h>
#endif

#include <sdbm.h>

/*
 * useful macros
 */
#define bad(x)          ((x).dptr == NULL || (x).dsize <= 0)
#define exhash(item)    sdbm_hash((item).dptr, (item).dsize)
#define ioerr(db)       ((db)->flags |= DBM_IOERR)

#define OFF_PAG(off)    (long) (off) * PBLKSIZ
#define OFF_DIR(off)    (long) (off) * DBLKSIZ

static long masks[] =
{
    000000000000, 000000000001, 000000000003, 000000000007,
    000000000017, 000000000037, 000000000077, 000000000177,
    000000000377, 000000000777, 000000001777, 000000003777,
    000000007777, 000000017777, 000000037777, 000000077777,
    000000177777, 000000377777, 000000777777, 000001777777,
    000003777777, 000007777777, 000017777777, 000037777777,
    000077777777, 000177777777, 000377777777, 000777777777,
    001777777777, 003777777777, 007777777777, 017777777777
};

datum   nullitem =
{NULL, 0};

typedef struct
{
    int     dirf;			/* directory file descriptor */
    int     pagf;			/* page file descriptor */
    int     flags;			/* status/error flags, see below */
    long    maxbno;			/* size of dirfile in bits */
    long    curbit;			/* current bit number */
    long    hmask;			/* current hash mask */
    long    blkptr;			/* current block for nextkey */
    int     keyptr;			/* current key for nextkey */
    long    blkno;			/* current page to read/write */
    long    pagbno;			/* current page in pagbuf */
    char   *pagbuf;			/* page file block buffer */
    long    dirbno;			/* current block in dirbuf */
    char   *dirbuf;			/* directory file block buffer */
}       DBM;


/* ************************* */

/*
 * sdbm - ndbm work-alike hashed database library
 * based on Per-Aake Larson's Dynamic Hashing algorithms. BIT 18 (1978).
 * author: oz@nexus.yorku.ca
 * status: public domain. keep it that way.
 *
 * hashing routine
 */

/*
 * polynomial conversion ignoring overflows
 * [this seems to work remarkably well, in fact better
 * then the ndbm hash function. Replace at your own risk]
 * use: 65599   nice.
 *      65587   even better.
 */
static long sdbm_hash (char *str, int len)
{
    unsigned long n = 0;

#ifdef DUFF
#define HASHC   n = *str++ + 65599 * n
    if (len > 0)
      {
	  int     loop = (len + 8 - 1) >> 3;

	  switch (len & (8 - 1))
	    {
	    case 0:
		do
		  {
		      HASHC;
	    case 7:
		      HASHC;
	    case 6:
		      HASHC;
	    case 5:
		      HASHC;
	    case 4:
		      HASHC;
	    case 3:
		      HASHC;
	    case 2:
		      HASHC;
	    case 1:
		      HASHC;
		  }
		while (--loop);
	    }

      }
#else
    while (len--)
	n = *str++ + 65599 * n;
#endif
    return n;
}

/*
 * check page sanity:
 * number of entries should be something
 * reasonable, and all offsets in the index should be in order.
 * this could be made more rigorous.
 */
static int chkpage (char *pag)
{
    int     n;
    int     off;
    short  *ino = (short *) pag;

    if ((n = ino[0]) < 0 || n > PBLKSIZ / sizeof (short))
	        return 0;

    if (n > 0)
      {
	  off = PBLKSIZ;
	  for (ino++; n > 0; ino += 2)
	    {
		if (ino[0] > off || ino[1] > off ||
		    ino[1] > ino[0])
		    return 0;
		off = ino[1];
		n -= 2;
	    }
      }
    return 1;
}

/*
 * search for the key in the page.
 * return offset index in the range 0 < i < n.
 * return 0 if not found.
 */
static int seepair (char *pag, int n, char *key, int siz)
{
    int     i;
    int     off = PBLKSIZ;
    short  *ino = (short *) pag;

    for (i = 1; i < n; i += 2)
      {
	  if (siz == off - ino[i] &&
	      memcmp (key, pag + ino[i], siz) == 0)
	      return i;
	  off = ino[i + 1];
      }
    return 0;
}

#ifdef SEEDUPS
static int duppair (char *pag, datum key)
{
    short  *ino = (short *) pag;

    return ino[0] > 0 && seepair (pag, ino[0], key.dptr, key.dsize) > 0;
}

#endif

/* ************************* */

/*
 * sdbm - ndbm work-alike hashed database library
 * based on Per-Aake Larson's Dynamic Hashing algorithms. BIT 18 (1978).
 * author: oz@nexus.yorku.ca
 * status: public domain.
 *
 * page-level routines
 */

/*
 * page format:
 *      +------------------------------+
 * ino  | n | keyoff | datoff | keyoff |
 *      +------------+--------+--------+
 *      | datoff | - - - ---->         |
 *      +--------+---------------------+
 *      |        F R E E A R E A       |
 *      +--------------+---------------+
 *      |  <---- - - - | data          |
 *      +--------+-----+----+----------+
 *      |  key   | data     | key      |
 *      +--------+----------+----------+
 *
 * calculating the offsets for free area:  if the number
 * of entries (ino[0]) is zero, the offset to the END of
 * the free area is the block size. Otherwise, it is the
 * nth (ino[ino[0]]) entry's offset.
 */

static int fitpair (char *pag, int need)
{
    int     n;
    int     off;
    int     avail;
    short  *ino = (short *) pag;

    off = ((n = ino[0]) > 0) ? ino[n] : PBLKSIZ;
    avail = off - (n + 1) * sizeof (short);
    need += 2 * sizeof (short);

    return need <= avail;
}

static void putpair (char *pag, datum key, datum val)
{
    int     n;
    int     off;
    short  *ino = (short *) pag;

    off = ((n = ino[0]) > 0) ? ino[n] : PBLKSIZ;
/*
 * enter the key first
 */
    off -= key.dsize;
    (void) memcpy (pag + off, key.dptr, key.dsize);
    ino[n + 1] = off;
/*
 * now the data
 */
    off -= val.dsize;
    (void) memcpy (pag + off, val.dptr, val.dsize);
    ino[n + 2] = off;
/*
 * adjust item count
 */
    ino[0] += 2;
}

static datum getpair (char *pag, datum key)
{
    int     i;
    int     n;
    datum   val;
    short  *ino = (short *) pag;

    if ((n = ino[0]) == 0)
	return nullitem;

    if ((i = seepair (pag, n, key.dptr, key.dsize)) == 0)
	return nullitem;

    val.dptr = pag + ino[i + 1];
    val.dsize = ino[i] - ino[i + 1];
    return val;
}

static datum getnkey (char *pag, int num)
{
    datum   key;
    int     off;
    short  *ino = (short *) pag;

    num = num * 2 - 1;
    if (ino[0] == 0 || num > ino[0])
	return nullitem;

    off = (num > 1) ? ino[num - 1] : PBLKSIZ;

    key.dptr = pag + ino[num];
    key.dsize = off - ino[num];

    return key;
}

static int delpair (char *pag, datum key)
{
    int     n;
    int     i;
    short  *ino = (short *) pag;

    if ((n = ino[0]) == 0)
	return 0;

    if ((i = seepair (pag, n, key.dptr, key.dsize)) == 0)
	return 0;
/*
 * found the key. if it is the last entry
 * [i.e. i == n - 1] we just adjust the entry count.
 * hard case: move all data down onto the deleted pair,
 * shift offsets onto deleted offsets, and adjust them.
 * [note: 0 < i < n]
 */
    if (i < n - 1)
      {
	  int     m;
	  char   *dst = pag + (i == 1 ? PBLKSIZ : ino[i - 1]);
	  char   *src = pag + ino[i + 1];
	  int     zoo = dst - src;

/*
 * shift data/keys down
 */
	  m = ino[i + 1] - ino[n];
#ifdef DUFF
#define MOVB    *--dst = *--src
	  if (m > 0)
	    {
		int     loop = (m + 8 - 1) >> 3;

		switch (m & (8 - 1))
		  {
		  case 0:
		      do
			{
			    MOVB;
		  case 7:
			    MOVB;
		  case 6:
			    MOVB;
		  case 5:
			    MOVB;
		  case 4:
			    MOVB;
		  case 3:
			    MOVB;
		  case 2:
			    MOVB;
		  case 1:
			    MOVB;
			}
		      while (--loop);
		  }
	    }
#else
	  dst -= m;
	  src -= m;
	  memmove (dst, src, m);
#endif
/*
 * adjust offset index up
 */
	  while (i < n - 1)
	    {
		ino[i] = ino[i + 2] + zoo;
		i++;
	    }
      }
    ino[0] -= 2;
    return 1;
}

static void splpage (char *pag, char *new, long sbit)
{
    datum   key;
    datum   val;

    int     n;
    int     off = PBLKSIZ;
    char    cur[PBLKSIZ];
    short  *ino = (short *) cur;

    (void) memcpy (cur, pag, PBLKSIZ);
    (void) memset (pag, 0, PBLKSIZ);
    (void) memset (new, 0, PBLKSIZ);

    n = ino[0];
    for (ino++; n > 0; ino += 2)
      {
	  key.dptr = cur + ino[0];
	  key.dsize = off - ino[0];
	  val.dptr = cur + ino[1];
	  val.dsize = ino[0] - ino[1];
/*
 * select the page pointer (by looking at sbit) and insert
 */
	  (void) putpair ((exhash (key) & sbit) ? new : pag, key, val);

	  off = ino[1];
	  n -= 2;
      }
}

static int getdbit (DBM * db, long dbit)
{
    long    c;
    long    dirb;

    c = dbit / BYTESIZ;
    dirb = c / DBLKSIZ;

    if (dirb != db->dirbno)
      {
	  int got;
	  if (lseek (db->dirf, OFF_DIR (dirb), SEEK_SET) < 0
	      || (got = read(db->dirf, db->dirbuf, DBLKSIZ)) < 0)
	      return 0;
	  if (got==0)
              memset(db->dirbuf,0,DBLKSIZ);
	  db->dirbno = dirb;
      }

    return db->dirbuf[c % DBLKSIZ] & (1 << dbit % BYTESIZ);
}

static int setdbit (DBM * db, long dbit)
{
    long    c;
    long    dirb;

    c = dbit / BYTESIZ;
    dirb = c / DBLKSIZ;

    if (dirb != db->dirbno)
      {
	  int got;
	  if (lseek (db->dirf, OFF_DIR (dirb), SEEK_SET) < 0
	      || (got = read(db->dirf, db->dirbuf, DBLKSIZ)) < 0)
	      return 0;
	  if (got==0)
              memset(db->dirbuf,0,DBLKSIZ);
	  db->dirbno = dirb;
      }

    db->dirbuf[c % DBLKSIZ] |= (1 << dbit % BYTESIZ);

#if 0
    if (dbit >= db->maxbno)
	db->maxbno += DBLKSIZ * BYTESIZ;
#else
    if (OFF_DIR((dirb+1))*BYTESIZ > db->maxbno)
        db->maxbno=OFF_DIR((dirb+1))*BYTESIZ;
#endif

    if (lseek (db->dirf, OFF_DIR (dirb), SEEK_SET) < 0
	|| write (db->dirf, db->dirbuf, DBLKSIZ) < 0)
	return 0;

    return 1;
}

/*
 * getnext - get the next key in the page, and if done with
 * the page, try the next page in sequence
 */
static datum getnext (DBM * db)
{
    datum   key;

    for (;;)
      {
	  db->keyptr++;
	  key = getnkey (db->pagbuf, db->keyptr);
	  if (key.dptr != NULL)
	      return key;
/*
 * we either run out, or there is nothing on this page..
 * try the next one... If we lost our position on the
 * file, we will have to seek.
 */
	  db->keyptr = 0;
	  if (db->pagbno != db->blkptr++)
	      if (lseek (db->pagf, OFF_PAG (db->blkptr), SEEK_SET) < 0)
		  break;
	  db->pagbno = db->blkptr;
	  if (read (db->pagf, db->pagbuf, PBLKSIZ) <= 0)
	      break;
	  if (!chkpage (db->pagbuf))
	      break;
      }

    return ioerr (db), nullitem;
}

/*
 * all important binary trie traversal
 */
static int getpage (DBM * db, long hash)
{
    int     hbit;
    long    dbit;
    long    pagb;

    dbit = 0;
    hbit = 0;
    while (dbit < db->maxbno && getdbit (db, dbit))
	dbit = 2 * dbit + ((hash & (1 << hbit++)) ? 2 : 1);

    db->curbit = dbit;
    db->hmask = masks[hbit];

    pagb = hash & db->hmask;
/*
 * see if the block we need is already in memory.
 * note: this lookaside cache has about 10% hit rate.
 */
    if (pagb != db->pagbno)
      {
/*
 * note: here, we assume a "hole" is read as 0s.
 * if not, must zero pagbuf first.
 */
	  if (lseek (db->pagf, OFF_PAG (pagb), SEEK_SET) < 0
	      || read (db->pagf, db->pagbuf, PBLKSIZ) < 0)
	      return 0;
	  if (!chkpage (db->pagbuf))
	      return 0;
	  db->pagbno = pagb;
      }
    return 1;
}

/*
 * makroom - make room by splitting the overfull page
 * this routine will attempt to make room for SPLTMAX times before
 * giving up.
 */
static int makroom (DBM * db, long hash, int need)
{
    long    newp;
    char    twin[PBLKSIZ];
    char   *pag = db->pagbuf;
    char   *new = twin;
    int     smax = SPLTMAX;

    do
      {
/*
 * split the current page
 */
	  (void) splpage (pag, new, db->hmask + 1);
/*
 * address of the new page
 */
	  newp = (hash & db->hmask) | (db->hmask + 1);

/*
 * write delay, read avoidence/cache shuffle:
 * select the page for incoming pair: if key is to go to the new page,
 * write out the previous one, and copy the new one over, thus making
 * it the current page. If not, simply write the new page, and we are
 * still looking at the page of interest. current page is not updated
 * here, as sdbm_store will do so, after it inserts the incoming pair.
 */
	  if (hash & (db->hmask + 1))
	    {
		if (lseek (db->pagf, OFF_PAG (db->pagbno), SEEK_SET) < 0
		    || write (db->pagf, db->pagbuf, PBLKSIZ) < 0)
		    return 0;
		db->pagbno = newp;
		(void) memcpy (pag, new, PBLKSIZ);
	    }
	  else if (lseek (db->pagf, OFF_PAG (newp), SEEK_SET) < 0
		   || write (db->pagf, new, PBLKSIZ) < 0)
	      return 0;

	  if (!setdbit (db, db->curbit))
	      return 0;
/*
 * see if we have enough room now
 */
	  if (fitpair (pag, need))
	      return 1;
/*
 * try again... update curbit and hmask as getpage would have
 * done. because of our update of the current page, we do not
 * need to read in anything. BUT we have to write the current
 * [deferred] page out, as the window of failure is too great.
 */
	  db->curbit = 2 * db->curbit +
	      ((hash & (db->hmask + 1)) ? 2 : 1);
	  db->hmask |= db->hmask + 1;

	  if (lseek (db->pagf, OFF_PAG (db->pagbno), SEEK_SET) < 0
	      || write (db->pagf, db->pagbuf, PBLKSIZ) < 0)
	      return 0;

      }
    while (--smax);
/*
 * if we are here, this is real bad news. After SPLTMAX splits,
 * we still cannot fit the key. say goodnight.
 */
#ifdef BADMESS
    (void) write (2, "sdbm: cannot insert after SPLTMAX attempts.\n", 44);
#endif
    return 0;

}

static SDBM *sdbm_prep (char *dirname, char *pagname, int flags, int mode)
{
    SDBM   *db;
    struct stat dstat;

    if ((db = (SDBM *) mymalloc (sizeof (SDBM))) == NULL)
	return errno = ENOMEM, (SDBM *) NULL;

    db->flags = 0;
    db->blkptr = 0;
    db->keyptr = 0;
/*
 * adjust user flags so that WRONLY becomes RDWR,
 * as required by this package. Also set our internal
 * flag for RDONLY if needed.
 */
    if (flags & O_WRONLY)
	flags = (flags & ~O_WRONLY) | O_RDWR;
    else if ((flags & 03) == O_RDONLY)
	db->flags = DBM_RDONLY;
#if defined(OS2) || defined(MSDOS) || defined(WIN32)
    flags |= O_BINARY;
#endif

/*
 * Make sure to ignore the O_EXCL option, as the file might exist due
 * to the locking.
 */
    flags &= ~O_EXCL;

/*
 * open the files in sequence, and stat the dirfile.
 * If we fail anywhere, undo everything, return NULL.
 */

    if ((db->pagf = open (pagname, flags, mode)) > -1)
      {
	  if ((db->dirf = open (dirname, flags, mode)) > -1)
	    {
/*
 * need the dirfile size to establish max bit number.
 */
		if (fstat (db->dirf, &dstat) == 0)
		  {
		      /*
                       * success
                       */
		      return db;
		  }
		msg_info ("closing dirf");
		(void) close (db->dirf);
	    }
	  msg_info ("closing pagf");
	  (void) close (db->pagf);
      }
    myfree ((char *) db);
    return (SDBM *) NULL;
}

static DBM *sdbm_internal_open (SDBM * sdbm)
{
    DBM    *db;
    struct stat dstat;

    if ((db = (DBM *) mymalloc (sizeof (DBM))) == NULL)
	return errno = ENOMEM, (DBM *) NULL;

    db->flags = sdbm->flags;
    db->hmask = 0;
    db->blkptr = sdbm->blkptr;
    db->keyptr = sdbm->keyptr;
    db->pagf = sdbm->pagf;
    db->dirf = sdbm->dirf;
    db->pagbuf = sdbm->pagbuf;
    db->dirbuf = sdbm->dirbuf;

/*
 * need the dirfile size to establish max bit number.
 */
    if (fstat (db->dirf, &dstat) == 0)
      {
/*
 * zero size: either a fresh database, or one with a single,
 * unsplit data page: dirpage is all zeros.
 */
	  db->dirbno = (!dstat.st_size) ? 0 : -1;
	  db->pagbno = -1;
	  db->maxbno = dstat.st_size * BYTESIZ;

	  (void) memset (db->pagbuf, 0, PBLKSIZ);
	  (void) memset (db->dirbuf, 0, DBLKSIZ);
	  return db;
      }
    myfree ((char *) db);
    return (DBM *) NULL;
}

static void sdbm_internal_close (DBM * db)
{
    if (db == NULL)
	errno = EINVAL;
    else
      {
	  myfree ((char *) db);
      }
}

datum   sdbm_fetch (SDBM * sdb, datum key)
{
    datum   retval;
    DBM    *db;

    if (sdb == NULL || bad (key))
	return errno = EINVAL, nullitem;

    if (!(db = sdbm_internal_open (sdb)))
	return errno = EINVAL, nullitem;

    if (getpage (db, exhash (key)))
      {
	  retval = getpair (db->pagbuf, key);
	  sdbm_internal_close (db);
	  return retval;
      }

    sdbm_internal_close (db);

    return ioerr (sdb), nullitem;
}

int     sdbm_delete (SDBM * sdb, datum key)
{
    int     retval;
    DBM    *db;

    if (sdb == NULL || bad (key))
	return errno = EINVAL, -1;
    if (sdbm_rdonly (sdb))
	return errno = EPERM, -1;

    if (!(db = sdbm_internal_open (sdb)))
	return errno = EINVAL, -1;

    if (getpage (db, exhash (key)))
      {
	  if (!delpair (db->pagbuf, key))
	      retval = -1;
/*
 * update the page file
 */
	  else if (lseek (db->pagf, OFF_PAG (db->pagbno), SEEK_SET) < 0
		   || write (db->pagf, db->pagbuf, PBLKSIZ) < 0)
	      retval = ioerr (sdb), -1;
	  else
	      retval = 0;
      }
    else
	retval = ioerr (sdb), -1;

    sdbm_internal_close (db);

    return retval;
}

int     sdbm_store (SDBM * sdb, datum key, datum val, int flags)
{
    int     need;
    int     retval;
    long    hash;
    DBM    *db;

    if (sdb == NULL || bad (key))
	return errno = EINVAL, -1;
    if (sdbm_rdonly (sdb))
	return errno = EPERM, -1;

    need = key.dsize + val.dsize;
/*
 * is the pair too big (or too small) for this database ??
 */
    if (need < 0 || need > PAIRMAX)
	return errno = EINVAL, -1;

    if (!(db = sdbm_internal_open (sdb)))
	return errno = EINVAL, -1;

    if (getpage (db, (hash = exhash (key))))
      {
/*
 * if we need to replace, delete the key/data pair
 * first. If it is not there, ignore.
 */
	  if (flags == DBM_REPLACE)
	      (void) delpair (db->pagbuf, key);
#ifdef SEEDUPS
	  else if (duppair (db->pagbuf, key))
	    {
		sdbm_internal_close (db);
		return 1;
	    }
#endif
/*
 * if we do not have enough room, we have to split.
 */
	  if (!fitpair (db->pagbuf, need))
	      if (!makroom (db, hash, need))
		{
		    sdbm_internal_close (db);
		    return ioerr (db), -1;
		}
/*
 * we have enough room or split is successful. insert the key,
 * and update the page file.
 */
	  (void) putpair (db->pagbuf, key, val);

	  if (lseek (db->pagf, OFF_PAG (db->pagbno), SEEK_SET) < 0
	      || write (db->pagf, db->pagbuf, PBLKSIZ) < 0)
	    {
		sdbm_internal_close (db);
		return ioerr (db), -1;
	    }
	  /*
           * success
           */
	  sdbm_internal_close (db);
	  return 0;
      }

    sdbm_internal_close (db);
    return ioerr (sdb), -1;
}

/*
 * the following two routines will break if
 * deletions aren't taken into account. (ndbm bug)
 */
datum   sdbm_firstkey (SDBM * sdb)
{
    datum   retval;
    DBM    *db;

    if (sdb == NULL)
	return errno = EINVAL, nullitem;

    if (!(db = sdbm_internal_open (sdb)))
	return errno = EINVAL, nullitem;

/*
 * start at page 0
 */
    if (lseek (db->pagf, OFF_PAG (0), SEEK_SET) < 0
	|| read (db->pagf, db->pagbuf, PBLKSIZ) < 0)
      {
	  sdbm_internal_close (db);
	  return ioerr (sdb), nullitem;
      }
    db->pagbno = 0;
    db->blkptr = 0;
    db->keyptr = 0;

    retval = getnext (db);
    sdb->blkptr = db->blkptr;
    sdb->keyptr = db->keyptr;
    sdbm_internal_close (db);
    return retval;
}

datum   sdbm_nextkey (SDBM * sdb)
{
    datum   retval;
    DBM    *db;

    if (sdb == NULL)
	return errno = EINVAL, nullitem;

    if (!(db = sdbm_internal_open (sdb)))
	return errno = EINVAL, nullitem;

    retval = getnext (db);
    sdb->blkptr = db->blkptr;
    sdb->keyptr = db->keyptr;
    sdbm_internal_close (db);
    return retval;
}

void    sdbm_close (SDBM * db)
{
    if (db == NULL)
	errno = EINVAL;
    else
      {
	  (void) close (db->dirf);
	  (void) close (db->pagf);
	  myfree ((char *) db);
      }
}

SDBM   *sdbm_open (char *file, int flags, int mode)
{
    SDBM   *db;
    char   *dirname;
    char   *pagname;
    int     n;

    if (file == NULL || !*file)
	return errno = EINVAL, (SDBM *) NULL;
/*
 * need space for two seperate filenames
 */
    n = strlen (file) * 2 + strlen (DIRFEXT) + strlen (PAGFEXT) + 2;

    if ((dirname = (char *) mymalloc ((unsigned) n)) == NULL)
	return errno = ENOMEM, (SDBM *) NULL;
/*
 * build the file names
 */
    dirname = strcat (strcpy (dirname, file), DIRFEXT);
    pagname = strcpy (dirname + strlen (dirname) + 1, file);
    pagname = strcat (pagname, PAGFEXT);

    db = sdbm_prep (dirname, pagname, flags, mode);
    myfree ((char *) dirname);
    return db;
}

