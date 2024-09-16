/**********************************************************************
  unicode.c -  Oniguruma (regular expression library)
**********************************************************************/
/*-
 * Copyright (c) 2002-2016  K.Kosako  <sndgk393 AT ybb DOT ne DOT jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "regint.h"

#define ONIGENC_IS_UNICODE_ISO_8859_1_CTYPE(code,ctype) \
  ((EncUNICODE_ISO_8859_1_CtypeTable[code] & CTYPE_TO_BIT(ctype)) != 0)

static const unsigned short EncUNICODE_ISO_8859_1_CtypeTable[256] = {
  0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008,
  0x4008, 0x428c, 0x4289, 0x4288, 0x4288, 0x4288, 0x4008, 0x4008,
  0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008,
  0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008, 0x4008,
  0x4284, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0,
  0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0,
  0x78b0, 0x78b0, 0x78b0, 0x78b0, 0x78b0, 0x78b0, 0x78b0, 0x78b0,
  0x78b0, 0x78b0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x41a0,
  0x41a0, 0x7ca2, 0x7ca2, 0x7ca2, 0x7ca2, 0x7ca2, 0x7ca2, 0x74a2,
  0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2,
  0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2, 0x74a2,
  0x74a2, 0x74a2, 0x74a2, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x51a0,
  0x41a0, 0x78e2, 0x78e2, 0x78e2, 0x78e2, 0x78e2, 0x78e2, 0x70e2,
  0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2,
  0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2, 0x70e2,
  0x70e2, 0x70e2, 0x70e2, 0x41a0, 0x41a0, 0x41a0, 0x41a0, 0x4008,
  0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0288, 0x0008, 0x0008,
  0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
  0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
  0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
  0x0284, 0x01a0, 0x00a0, 0x00a0, 0x00a0, 0x00a0, 0x00a0, 0x00a0,
  0x00a0, 0x00a0, 0x30e2, 0x01a0, 0x00a0, 0x00a8, 0x00a0, 0x00a0,
  0x00a0, 0x00a0, 0x10a0, 0x10a0, 0x00a0, 0x30e2, 0x00a0, 0x01a0,
  0x00a0, 0x10a0, 0x30e2, 0x01a0, 0x10a0, 0x10a0, 0x10a0, 0x01a0,
  0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2,
  0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2,
  0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x00a0,
  0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x34a2, 0x30e2,
  0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2,
  0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2,
  0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x00a0,
  0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2, 0x30e2
};

#ifdef USE_UNICODE_PROPERTIES
#include "unicode_property_data.c"
#else
#include "unicode_property_data_posix.c"
#endif

#include "st.h"

#define USER_DEFINED_PROPERTY_MAX_NUM  20

typedef struct {
  int ctype;
  OnigCodePoint* ranges;
} UserDefinedPropertyValue;

static int UserDefinedPropertyNum;
static UserDefinedPropertyValue
UserDefinedPropertyRanges[USER_DEFINED_PROPERTY_MAX_NUM];
static st_table* UserDefinedPropertyTable;

extern int
onig_unicode_define_user_property(const char* name, OnigCodePoint* ranges)
{
  UserDefinedPropertyValue* e;
  int i;
  int n;
  int len;
  int c;
  char* s;

  if (UserDefinedPropertyNum >= USER_DEFINED_PROPERTY_MAX_NUM)
    return ONIGERR_TOO_MANY_USER_DEFINED_OBJECTS;

  len = strlen(name);
  if (len >= PROPERTY_NAME_MAX_SIZE)
    return ONIGERR_TOO_LONG_PROPERTY_NAME;

  s = (char* )xmalloc(len + 1);
  if (s == 0)
    return ONIGERR_MEMORY;

  n = 0;
  for (i = 0; i < len; i++) {
    c = name[i];
    if (c <= 0 || c >= 0x80) {
      xfree(s);
      return ONIGERR_INVALID_CHAR_PROPERTY_NAME;
    }

    if (c != ' ' && c != '-' && c != '_') {
      s[n] = c;
      n++;
    }
  }
  s[n] = '\0';

  if (UserDefinedPropertyTable == 0) {
    UserDefinedPropertyTable = onig_st_init_strend_table_with_size(10);
  }

  e = UserDefinedPropertyRanges + UserDefinedPropertyNum;
  e->ctype = CODE_RANGES_NUM + UserDefinedPropertyNum;
  e->ranges = ranges;
  onig_st_insert_strend(UserDefinedPropertyTable,
                        (const UChar* )s, (const UChar* )s + n,
                        (hash_data_type )((void* )e));

  UserDefinedPropertyNum++;
  return 0;
}

extern int
onigenc_unicode_is_code_ctype(OnigCodePoint code, unsigned int ctype)
{
  if (
#ifdef USE_UNICODE_PROPERTIES
      ctype <= ONIGENC_MAX_STD_CTYPE &&
#endif
      code < 256) {
    return ONIGENC_IS_UNICODE_ISO_8859_1_CTYPE(code, ctype);
  }

  if (ctype >= CODE_RANGES_NUM) {
    int index = ctype - CODE_RANGES_NUM;
    if (index < UserDefinedPropertyNum)
      return onig_is_in_code_range((UChar* )UserDefinedPropertyRanges[index].ranges, code);
    else
      return ONIGERR_TYPE_BUG;
  }

  return onig_is_in_code_range((UChar* )CodeRanges[ctype], code);
}


extern int
onigenc_unicode_ctype_code_range(int ctype, const OnigCodePoint* ranges[])
{
  if (ctype >= CODE_RANGES_NUM) {
    int index = ctype - CODE_RANGES_NUM;
    if (index < UserDefinedPropertyNum) {
      *ranges = UserDefinedPropertyRanges[index].ranges;
      return 0;
    }
    else
      return ONIGERR_TYPE_BUG;
  }

  *ranges = CodeRanges[ctype];
  return 0;
}

extern int
onigenc_utf16_32_get_ctype_code_range(OnigCtype ctype, OnigCodePoint* sb_out,
                                      const OnigCodePoint* ranges[])
{
  *sb_out = 0x00;
  return onigenc_unicode_ctype_code_range(ctype, ranges);
}

extern int
onigenc_unicode_property_name_to_ctype(OnigEncoding enc, UChar* name, UChar* end)
{
  int len;
  UChar *p;
  OnigCodePoint code;
  const struct PropertyNameCtype* pc;
  char buf[PROPERTY_NAME_MAX_SIZE];

  p = name;
  len = 0;
  while (p < end) {
    code = ONIGENC_MBC_TO_CODE(enc, p, end);
    if (code >= 0x80)
      return ONIGERR_INVALID_CHAR_PROPERTY_NAME;

    if (code != ' ' && code != '-' && code != '_') {
      buf[len++] = (char )code;
      if (len >= PROPERTY_NAME_MAX_SIZE)
        return ONIGERR_INVALID_CHAR_PROPERTY_NAME;
    }

    p += enclen(enc, p);
  }

  buf[len] = 0;

  if (UserDefinedPropertyTable != 0) {
    UserDefinedPropertyValue* e;
    e = (UserDefinedPropertyValue* )NULL;
    onig_st_lookup_strend(UserDefinedPropertyTable,
			  (const UChar* )buf, (const UChar* )buf + len,
			  (hash_data_type* )((void* )(&e)));
    if (e != 0) {
      return e->ctype;
    }
  }

  pc = unicode_lookup_property_name(buf, len);
  if (pc != 0) {
    /* fprintf(stderr, "LOOKUP: %s: %d\n", buf, pc->ctype); */
#ifndef USE_UNICODE_PROPERTIES
    if (pc->ctype > ONIGENC_MAX_STD_CTYPE)
      return ONIGERR_INVALID_CHAR_PROPERTY_NAME;
#endif

    return pc->ctype;
  }

  return ONIGERR_INVALID_CHAR_PROPERTY_NAME;
}

/* for use macros in unicode_fold_data.c */
#include "unicode_fold_data.c"


extern int
onigenc_unicode_mbc_case_fold(OnigEncoding enc,
    OnigCaseFoldType flag ARG_UNUSED, const UChar** pp, const UChar* end,
    UChar* fold)
{
  const struct ByUnfoldKey* buk;

  OnigCodePoint code;
  int i, len, rlen;
  const UChar *p = *pp;

  code = ONIGENC_MBC_TO_CODE(enc, p, end);
  len = enclen(enc, p);
  *pp += len;

#ifdef USE_UNICODE_CASE_FOLD_TURKISH_AZERI
  if ((flag & ONIGENC_CASE_FOLD_TURKISH_AZERI) != 0) {
    if (code == 0x0130) {
      return ONIGENC_CODE_TO_MBC(enc, 0x0069, fold);
    }
#if 0
    if (code == 0x0049) {
      return ONIGENC_CODE_TO_MBC(enc, 0x0131, fold);
    }
#endif
  }
#endif

  buk = unicode_unfold_key(code);
  if (buk != 0) {
    if (buk->fold_len == 1) {
      return ONIGENC_CODE_TO_MBC(enc, *FOLDS1_FOLD(buk->index), fold);
    }
    else {
      OnigCodePoint* addr;

      FOLDS_FOLD_ADDR_BUK(buk, addr);
      rlen = 0;
      for (i = 0; i < buk->fold_len; i++) {
        OnigCodePoint c = addr[i];
        len = ONIGENC_CODE_TO_MBC(enc, c, fold);
        fold += len;
        rlen += len;
      }
      return rlen;
    }
  }

  for (i = 0; i < len; i++) {
    *fold++ = *p++;
  }
  return len;
}

static int
apply_case_fold1(int from, int to, OnigApplyAllCaseFoldFunc f, void* arg)
{
  int i, j, k, n, r;

  for (i = from; i < to; ) {
    OnigCodePoint fold = *FOLDS1_FOLD(i);
    n = FOLDS1_UNFOLDS_NUM(i);
    for (j = 0; j < n; j++) {
      OnigCodePoint unfold = FOLDS1_UNFOLDS(i)[j];

      r = (*f)(fold, &unfold, 1, arg);
      if (r != 0) return r;
      r = (*f)(unfold, &fold, 1, arg);
      if (r != 0) return r;

      for (k = 0; k < j; k++) {
        OnigCodePoint unfold2 = FOLDS1_UNFOLDS(i)[k];
        r = (*f)(unfold, &unfold2, 1, arg);
        if (r != 0) return r;
        r = (*f)(unfold2, &unfold, 1, arg);
        if (r != 0) return r;
      }
    }

    i = FOLDS1_NEXT_INDEX(i);
  }

  return 0;
}

static int
apply_case_fold2(int from, int to, OnigApplyAllCaseFoldFunc f, void* arg)
{
  int i, j, k, n, r;

  for (i = from; i < to; ) {
    OnigCodePoint* fold = FOLDS2_FOLD(i);
    n = FOLDS2_UNFOLDS_NUM(i);
    for (j = 0; j < n; j++) {
      OnigCodePoint unfold = FOLDS2_UNFOLDS(i)[j];

      r = (*f)(unfold, fold, 2, arg);
      if (r != 0) return r;

      for (k = 0; k < j; k++) {
        OnigCodePoint unfold2 = FOLDS2_UNFOLDS(i)[k];
        r = (*f)(unfold, &unfold2, 1, arg);
        if (r != 0) return r;
        r = (*f)(unfold2, &unfold, 1, arg);
        if (r != 0) return r;
      }
    }

    i = FOLDS2_NEXT_INDEX(i);
  }

  return 0;
}

static int
apply_case_fold3(int from, int to, OnigApplyAllCaseFoldFunc f, void* arg)
{
  int i, j, k, n, r;

  for (i = from; i < to; ) {
    OnigCodePoint* fold = FOLDS3_FOLD(i);
    n = FOLDS3_UNFOLDS_NUM(i);
    for (j = 0; j < n; j++) {
      OnigCodePoint unfold = FOLDS3_UNFOLDS(i)[j];

      r = (*f)(unfold, fold, 3, arg);
      if (r != 0) return r;

      for (k = 0; k < j; k++) {
        OnigCodePoint unfold2 = FOLDS3_UNFOLDS(i)[k];
        r = (*f)(unfold, &unfold2, 1, arg);
        if (r != 0) return r;
        r = (*f)(unfold2, &unfold, 1, arg);
        if (r != 0) return r;
      }
    }

    i = FOLDS3_NEXT_INDEX(i);
  }

  return 0;
}

extern int
onigenc_unicode_apply_all_case_fold(OnigCaseFoldType flag,
				    OnigApplyAllCaseFoldFunc f, void* arg)
{
  int r;

  r = apply_case_fold1(0, FOLDS1_NORMAL_END_INDEX, f, arg);
  if (r != 0) return r;

#ifdef USE_UNICODE_CASE_FOLD_TURKISH_AZERI
  if ((flag & ONIGENC_CASE_FOLD_TURKISH_AZERI) != 0) {
    code = 0x0131;
    r = (*f)(0x0049, &code, 1, arg);
    if (r != 0) return r;
    code = 0x0049;
    r = (*f)(0x0131, &code, 1, arg);
    if (r != 0) return r;

    code = 0x0130;
    r = (*f)(0x0069, &code, 1, arg);
    if (r != 0) return r;
    code = 0x0069;
    r = (*f)(0x0130, &code, 1, arg);
    if (r != 0) return r;
  }
  else {
#endif
    r = apply_case_fold1(FOLDS1_NORMAL_END_INDEX, FOLDS1_END_INDEX, f, arg);
    if (r != 0) return r;
#ifdef USE_UNICODE_CASE_FOLD_TURKISH_AZERI
  }
#endif

  if ((flag & INTERNAL_ONIGENC_CASE_FOLD_MULTI_CHAR) == 0)
    return 0;

  r = apply_case_fold2(0, FOLDS2_NORMAL_END_INDEX, f, arg);
  if (r != 0) return r;

#ifdef USE_UNICODE_CASE_FOLD_TURKISH_AZERI
  if ((flag & ONIGENC_CASE_FOLD_TURKISH_AZERI) == 0) {
#endif
    r = apply_case_fold2(FOLDS2_NORMAL_END_INDEX, FOLDS2_END_INDEX, f, arg);
    if (r != 0) return r;
#ifdef USE_UNICODE_CASE_FOLD_TURKISH_AZERI
  }
#endif

  r = apply_case_fold3(0, FOLDS3_NORMAL_END_INDEX, f, arg);
  if (r != 0) return r;

  return 0;
}

extern int
onigenc_unicode_get_case_fold_codes_by_str(OnigEncoding enc,
    OnigCaseFoldType flag, const OnigUChar* p, const OnigUChar* end,
    OnigCaseFoldCodeItem items[])
{
  int n, m, i, j, k, len;
  OnigCodePoint code, codes[3];
  const struct ByUnfoldKey* buk;

  n = 0;

  code = ONIGENC_MBC_TO_CODE(enc, p, end);
  len = enclen(enc, p);

#ifdef USE_UNICODE_CASE_FOLD_TURKISH_AZERI
  if ((flag & ONIGENC_CASE_FOLD_TURKISH_AZERI) != 0) {
    if (code == 0x0049) {
      items[0].byte_len = len;
      items[0].code_len = 1;
      items[0].code[0]  = 0x0131;
      return 1;
    }
    else if (code == 0x0130) {
      items[0].byte_len = len;
      items[0].code_len = 1;
      items[0].code[0]  = 0x0069;
      return 1;
    }
    else if (code == 0x0131) {
      items[0].byte_len = len;
      items[0].code_len = 1;
      items[0].code[0]  = 0x0049;
      return 1;
    }
    else if (code == 0x0069) {
      items[0].byte_len = len;
      items[0].code_len = 1;
      items[0].code[0]  = 0x0130;
      return 1;
    }
  }
#endif

  buk = unicode_unfold_key(code);
  if (buk != 0) {
    if (buk->fold_len == 1) {
      int un;
      items[0].byte_len = len;
      items[0].code_len = 1;
      items[0].code[0]  = *FOLDS1_FOLD(buk->index);
      n++;

      un = FOLDS1_UNFOLDS_NUM(buk->index);
      for (i = 0; i < un; i++) {
        OnigCodePoint unfold = FOLDS1_UNFOLDS(buk->index)[i];
        if (unfold != code) {
          items[n].byte_len = len;
          items[n].code_len = 1;
          items[n].code[0]  = unfold;
          n++;
        }
      }
      code = items[0].code[0]; // for multi-code to unfold search.
    }
    else if ((flag & INTERNAL_ONIGENC_CASE_FOLD_MULTI_CHAR) != 0) {
      OnigCodePoint cs[3][4];
      int fn, ncs[3];

      if (buk->fold_len == 2) {
        m = FOLDS2_UNFOLDS_NUM(buk->index);
        for (i = 0; i < m; i++) {
          OnigCodePoint unfold = FOLDS2_UNFOLDS(buk->index)[i];
          if (unfold == code) continue;

          items[n].byte_len = len;
          items[n].code_len = 1;
          items[n].code[0]  = unfold;
          n++;
        }

        for (fn = 0; fn < 2; fn++) {
          int index;
          cs[fn][0] = FOLDS2_FOLD(buk->index)[fn];
          index = unicode_fold1_key(&cs[fn][0]);
          if (index >= 0) {
            int m = FOLDS1_UNFOLDS_NUM(index);
            for (i = 0; i < m; i++) {
              cs[fn][i+1] = FOLDS1_UNFOLDS(index)[i];
            }
            ncs[fn] = m + 1;
          }
          else
            ncs[fn] = 1;
        }

        for (i = 0; i < ncs[0]; i++) {
          for (j = 0; j < ncs[1]; j++) {
            items[n].byte_len = len;
            items[n].code_len = 2;
            items[n].code[0]  = cs[0][i];
            items[n].code[1]  = cs[1][j];
            n++;
          }
        }
      }
      else { /* fold_len == 3 */
        m = FOLDS3_UNFOLDS_NUM(buk->index);
        for (i = 0; i < m; i++) {
          OnigCodePoint unfold = FOLDS3_UNFOLDS(buk->index)[i];
          if (unfold == code) continue;

          items[n].byte_len = len;
          items[n].code_len = 1;
          items[n].code[0]  = unfold;
          n++;
        }

        for (fn = 0; fn < 3; fn++) {
          int index;
          cs[fn][0] = FOLDS3_FOLD(buk->index)[fn];
          index = unicode_fold1_key(&cs[fn][0]);
          if (index >= 0) {
            int m = FOLDS1_UNFOLDS_NUM(index);
            for (i = 0; i < m; i++) {
              cs[fn][i+1] = FOLDS1_UNFOLDS(index)[i];
            }
            ncs[fn] = m + 1;
          }
          else
            ncs[fn] = 1;
        }

        for (i = 0; i < ncs[0]; i++) {
          for (j = 0; j < ncs[1]; j++) {
            for (k = 0; k < ncs[2]; k++) {
              items[n].byte_len = len;
              items[n].code_len = 3;
              items[n].code[0]  = cs[0][i];
              items[n].code[1]  = cs[1][j];
              items[n].code[2]  = cs[2][k];
              n++;
            }
          }
        }
      }

      /* multi char folded code is not head of another folded multi char */
      return n;
    }
  }
  else {
    int index = unicode_fold1_key(&code);
    if (index >= 0) {
      int m = FOLDS1_UNFOLDS_NUM(index);
      for (i = 0; i < m; i++) {
        items[n].byte_len = len;
        items[n].code_len = 1;
        items[n].code[0]  = FOLDS1_UNFOLDS(index)[i];
        n++;
      }
    }
  }

  if ((flag & INTERNAL_ONIGENC_CASE_FOLD_MULTI_CHAR) == 0)
    return n;

  p += len;
  if (p < end) {
    int clen;
    int index;

    codes[0] = code;
    code = ONIGENC_MBC_TO_CODE(enc, p, end);

    buk = unicode_unfold_key(code);
    if (buk != 0 && buk->fold_len == 1) {
      codes[1] = *FOLDS1_FOLD(buk->index);
    }
    else
      codes[1] = code;

    clen = enclen(enc, p);
    len += clen;

    index = unicode_fold2_key(codes);
    if (index >= 0) {
      m = FOLDS2_UNFOLDS_NUM(index);
      for (i = 0; i < m; i++) {
        items[n].byte_len = len;
        items[n].code_len = 1;
        items[n].code[0]  = FOLDS2_UNFOLDS(index)[i];
        n++;
      }
    }

    p += clen;
    if (p < end) {
      code = ONIGENC_MBC_TO_CODE(enc, p, end);
      buk = unicode_unfold_key(code);
      if (buk != 0 && buk->fold_len == 1) {
        codes[2] = *FOLDS1_FOLD(buk->index);
      }
      else
        codes[2] = code;

      clen = enclen(enc, p);
      len += clen;

      index = unicode_fold3_key(codes);
      if (index >= 0) {
        m = FOLDS3_UNFOLDS_NUM(index);
        for (i = 0; i < m; i++) {
          items[n].byte_len = len;
          items[n].code_len = 1;
          items[n].code[0]  = FOLDS3_UNFOLDS(index)[i];
          n++;
        }
      }
    }
  }

  return n;
}
