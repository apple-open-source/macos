/*
 * @OSF_COPYRIGHT@
 */
/*
 * HISTORY
 * $Log: mig_machine.h,v $
 * Revision 1.1  2000/01/11 00:36:20  wsanchez
 * Initial revision
 *
 * Revision 1.2  1999/01/19 03:36:30  wsanchez
 * Make main branch match bootstrap_cmds-16 (Mac OS X)
 *
 * Revision 1.1.2.1  1998/11/07 23:57:18  mwatson
 * New mig for Beaker/BHX.
 *
 * Revision 1.1.7.2  1996/02/12  16:48:09  emcmanus
 *  Define PACK_MESSAGES as TRUE.
 *  [1996/02/09  11:47:14  emcmanus]
 *
 * Revision 1.1.7.1  1995/01/06  21:06:21  devrcs
 *  mk6 CR668 - 1.3b26 merge
 *  [1994/10/27  18:50:33  duthie]
 *
 * Revision 1.1.2.1  1994/05/09  18:36:48  tmt
 *  New file for machine-dependent padding/rounding/size.
 *  [1994/05/09  18:27:48  tmt]
 *
 * $EndLog$
 */

#define machine_alignment(SZ,ESZ)   \
  (((SZ) = ((SZ) + 3) & ~3), (SZ) += (ESZ))

#define machine_padding(BYTES)  \
  ((BYTES & 3) ? (4 - (BYTES & 3)) : 0)

#ifndef NBBY
#define NBBY  8
#endif

#define PACK_MESSAGES TRUE
