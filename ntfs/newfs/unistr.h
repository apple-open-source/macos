/*
 * unistr.h - Exports for Unicode string handling. 
 *
 * Copyright (c) 2000-2004 Anton Altaparmakov
 * Copyright (c) 2008-2012 Tuxera Inc.
 *  
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_UNISTR_H
#define _NTFS_UNISTR_H

#include "types.h"
#include "layout.h"

extern BOOL ntfs_names_are_equal(const ntfschar *s1, size_t s1_len,
		const ntfschar *s2, size_t s2_len, const IGNORE_CASE_BOOL ic,
		const ntfschar *upcase, const u32 upcase_size);

extern int ntfs_names_full_collate(const ntfschar *name1, const u32 name1_len,
		const ntfschar *name2, const u32 name2_len,
		const IGNORE_CASE_BOOL ic,
		const ntfschar *upcase, const u32 upcase_len);

extern int ntfs_ucsncmp(const ntfschar *s1, const ntfschar *s2, size_t n);

extern int ntfs_ucsncasecmp(const ntfschar *s1, const ntfschar *s2, size_t n,
		const ntfschar *upcase, const u32 upcase_size);

extern u32 ntfs_ucsnlen(const ntfschar *s, u32 maxlen);

extern ntfschar *ntfs_ucsndup(const ntfschar *s, u32 maxlen);

extern int ntfs_ucstombs(const ntfschar *ins, const int ins_len, char **outs,
		int outs_len);
extern int ntfs_mbstoucs(const char *ins, ntfschar **outs);

extern void ntfs_upcase_table_build(ntfschar *uc, u32 uc_len);

extern ntfschar *ntfs_str2ucs(const char *s, int *len);

extern void ntfs_ucsfree(ntfschar *ucs);

#if defined(__APPLE__) || defined(__DARWIN__)
/**
 * Mac OS X only.
 * 
 * Normalizes the input string "utf8_string" to one of the normalization forms NFD or NFC.
 * The parameter "composed" decides whether output should be in composed, NFC, form
 * (composed == 1) or decomposed, NFD, form (composed == 0).
 * Input is assumed to be properly UTF-8 encoded and null-terminated. Output will be a newly
 * ntfs_calloc'ed string encoded in UTF-8. It is the callers responsibility to free(...) the
 * allocated string when it's no longer needed.
 *
 * @param utf8_string the input string, which may be in any normalization form.
 * @param target a pointer where the resulting string will be stored.
 * @param composed decides which composition form to normalize the input string to. 0 means
 *        composed form (NFC), 1 means decomposed form (NFD).
 * @return -1 if the normalization failed for some reason, otherwise the length of the
 *         normalized string stored in target.
 */
extern int ntfs_macosx_normalize_utf8(const char *utf8_string, char **target, int composed);
#endif /* defined(__APPLE__) || defined(__DARWIN__) */

#endif /* defined _NTFS_UNISTR_H */
