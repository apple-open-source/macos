/* 
 * tclLoadNone.c --
 *
 *	This procedure provides a version of the TclLoadFile for use
 *	in systems that don't support dynamic loading; it just returns
 *	an error.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclLoadNone.c 1.5 96/02/15 11:43:01
 */

#include "tcl.h"
#include "compat/dlfcn.h"

extern int adler32 (); 
extern int compress (); 
extern int crc32 (); 
extern int get_crc_table (); 
extern int gzclose (); 
extern int gzdopen (); 
extern int gzerror (); 
extern int gzflush (); 
extern int gzopen (); 
extern int gzread (); 
extern int gzwrite (); 
extern int uncompress (); 
extern int deflate (); 
extern int _tr_align (); 
extern int _tr_flush_block (); 
extern int _tr_init (); 
extern int _tr_stored_block (); 
extern int _tr_tally (); 
extern int zcalloc (); 
extern int zcfree (); 
extern int zlib (); 
extern int inflate (); 
extern int inflate_blocks (); 
extern int inflate_blocks_free (); 
extern int inflate_blocks_new (); 
extern int inflate_blocks_reset (); 
extern int inflate_set_dictionary (); 
extern int inflate_trees_bits (); 
extern int inflate_trees_dynamic (); 
extern int inflate_trees_fixed (); 
extern int inflate_trees_free (); 
extern int inflate_codes (); 
extern int inflate_codes_free (); 
extern int inflate_codes_new (); 
extern int inflate_flush (); 
extern int inflate_fast (); 
extern int des_3cbc_encrypt (); 
extern int des_cbc_cksum (); 
extern int des_cbc_encrypt (); 
extern int des_ncbc_encrypt (); 
extern int des_pcbc_encrypt (); 
extern int des_quad_cksum (); 
extern int des_ede3_cfb64_encrypt (); 
extern int des_cfb64_encrypt (); 
extern int des_cfb_encrypt (); 
extern int des_ecb3_encrypt (); 
extern int des_ecb_encrypt (); 
extern int des_encrypt (); 
extern int des_encrypt2 (); 
extern int des_ede3_cbc_encrypt (); 
extern int des_enc_read (); 
extern int des_enc_write (); 
extern int crypt (); 
extern int des_ede3_ofb64_encrypt (); 
extern int des_ofb64_encrypt (); 
extern int des_ofb_encrypt (); 
extern int des_random_key (); 
extern int des_random_seed (); 
extern int des_read_2passwords (); 
extern int des_read_password (); 
extern int des_read_pw_string (); 
extern int des_is_weak_key (); 
extern int des_key_sched (); 
extern int des_set_key (); 
extern int des_set_odd_parity (); 
extern int _des_crypt (); 
extern int des_string_to_2keys (); 
extern int des_string_to_key (); 
extern int des_cblock_print_file (); 

static struct {
  char * name;
  int (*value)();
}dictionary [] = {
  { "adler32", adler32 },
  { "compress", compress },
  { "crc32", crc32 },
  { "get_crc_table", get_crc_table },
  { "gzclose", gzclose },
  { "gzdopen", gzdopen },
  { "gzerror", gzerror },
  { "gzflush", gzflush },
  { "gzopen", gzopen },
  { "gzread", gzread },
  { "gzwrite", gzwrite },
  { "uncompress", uncompress },
  { "deflate", deflate },
  { "_tr_align", _tr_align },
  { "_tr_flush_block", _tr_flush_block },
  { "_tr_init", _tr_init },
  { "_tr_stored_block", _tr_stored_block },
  { "_tr_tally", _tr_tally },
  { "zcalloc", zcalloc },
  { "zcfree", zcfree },
  { "zlib", zlib },
  { "inflate", inflate },
  { "inflate_blocks", inflate_blocks },
  { "inflate_blocks_free", inflate_blocks_free },
  { "inflate_blocks_new", inflate_blocks_new },
  { "inflate_blocks_reset", inflate_blocks_reset },
  { "inflate_set_dictionary", inflate_set_dictionary },
  { "inflate_trees_bits", inflate_trees_bits },
  { "inflate_trees_dynamic", inflate_trees_dynamic },
  { "inflate_trees_fixed", inflate_trees_fixed },
  { "inflate_trees_free", inflate_trees_free },
  { "inflate_codes", inflate_codes },
  { "inflate_codes_free", inflate_codes_free },
  { "inflate_codes_new", inflate_codes_new },
  { "inflate_flush", inflate_flush },
  { "inflate_fast", inflate_fast },
  { "des_3cbc_encrypt", des_3cbc_encrypt },
  { "des_cbc_cksum", des_cbc_cksum },
  { "des_cbc_encrypt", des_cbc_encrypt },
  { "des_ncbc_encrypt", des_ncbc_encrypt },
  { "des_pcbc_encrypt", des_pcbc_encrypt },
  { "des_quad_cksum", des_quad_cksum },
  { "des_ede3_cfb64_encrypt", des_ede3_cfb64_encrypt },
  { "des_cfb64_encrypt", des_cfb64_encrypt },
  { "des_cfb_encrypt", des_cfb_encrypt },
  { "des_ecb3_encrypt", des_ecb3_encrypt },
  { "des_ecb_encrypt", des_ecb_encrypt },
  { "des_encrypt", des_encrypt },
  { "des_encrypt2", des_encrypt2 },
  { "des_ede3_cbc_encrypt", des_ede3_cbc_encrypt },
  { "des_enc_read", des_enc_read },
  { "des_enc_write", des_enc_write },
  { "crypt", crypt },
  { "des_ede3_ofb64_encrypt", des_ede3_ofb64_encrypt },
  { "des_ofb64_encrypt", des_ofb64_encrypt },
  { "des_ofb_encrypt", des_ofb_encrypt },
  { "des_random_key", des_random_key },
  { "des_random_seed", des_random_seed },
  { "des_read_2passwords", des_read_2passwords },
  { "des_read_password", des_read_password },
  { "des_read_pw_string", des_read_pw_string },
  { "des_is_weak_key", des_is_weak_key },
  { "des_key_sched", des_key_sched },
  { "des_set_key", des_set_key },
  { "des_set_odd_parity", des_set_odd_parity },
  { "_des_crypt", _des_crypt },
  { "des_string_to_2keys", des_string_to_2keys },
  { "des_string_to_key", des_string_to_key },
  { "des_cblock_print_file", des_cblock_print_file },
  { 0, 0 }
};

/*
 *----------------------------------------------------------------------
 *
 * dlopen  --
 * dlsym   --
 * dlerror --
 * dlclose --
 *
 *	Dummy functions, in case our system doesn't support
 *	dynamic loading.
 *
 * Results:
 *	NULL for dlopen() and dlsym(). Error for other functions.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */
VOID *dlopen(path, mode)
    CONST char *path;
    int mode;
{
    return (VOID *) 1;
}

VOID *dlsym(handle, symbol)
    VOID *handle;
    CONST char *symbol;
{
    int i;
    for (i = 0; dictionary [i] . name != 0; ++i) {
      if (!strcmp (symbol, dictionary [i] . name)) {
	return (VOID *) dictionary [i].value;
      }
    }
    return (VOID *) NULL;
}

char *dlerror()
{
    return "dynamic loading is not currently available on this system";
}

int dlclose(handle)
    VOID *handle;
{
    return 0;
}
