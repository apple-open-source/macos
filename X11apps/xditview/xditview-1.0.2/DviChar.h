/*
 * DviChar.h
 *
 * descriptions for mapping dvi names to
 * font indexes and back.  Dvi fonts are all
 * 256 elements (actually only 256-32 are usable).
 *
 * The encoding names are taken from X -
 * case insensitive, a dash seperating the
 * CharSetRegistry from the CharSetEncoding
 */
/* $XFree86$ */

#ifndef _DVICHAR_H_
#define _DVICHAR_H_

#include "Dvi.h"

# define DVI_MAX_SYNONYMS	10
# define DVI_MAP_SIZE		256
# define DVI_HASH_SIZE		256
# define DVI_MAX_LIGATURES	16

typedef struct _dviCharNameHash {
	struct _dviCharNameHash	*next;
	char			*name;
	int			position;
} DviCharNameHash;

typedef struct _dviCharNameMap {
    char		*encoding;
    int			special;
    char		*dvi_names[DVI_MAP_SIZE][DVI_MAX_SYNONYMS];
    char		*ligatures[DVI_MAX_LIGATURES][2];
    DviCharNameHash	*buckets[DVI_HASH_SIZE];
} DviCharNameMap;

extern DviCharNameMap	*DviFindMap (char *);
extern void		DviRegisterMap (DviCharNameMap *);
#ifdef NOTDEF
extern char		*DviCharName (DviCharNameMap *, int, int);
#else
#define DviCharName(map,index,synonym)	((map)->dvi_names[index][synonym])
#endif
extern int		DviCharIndex (DviCharNameMap *, char *);
extern unsigned char	*DviCharIsLigature (DviCharNameMap *, char *);
extern void		ResetFonts (DviWidget);

#endif
