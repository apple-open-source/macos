/*
 * @OSF_COPYRIGHT@
 */
/*
 * HISTORY
 * $Log: mig_machine.h,v $
 * Revision 1.1  2000/01/11 00:36:20  wsanchez
 * Initial revision
 *
 * Revision 1.2  1999/01/19 03:36:31  wsanchez
 * Make main branch match bootstrap_cmds-16 (Mac OS X)
 *
 * Revision 1.1.2.1  1998/11/07 23:57:19  mwatson
 * New mig for Beaker/BHX.
 *
 * Revision 1.1.9.1  1996/12/18  15:33:11  stephen
 *  nmklinux -> cmk
 *  [1996/12/18  15:27:19  stephen]
 *
 * Revision 1.1.6.2  1996/04/22  08:41:43  stephen
 *  Added PACK_MESSAGES definition
 *  [1996/04/22  08:33:23  stephen]
 *
 * Revision 1.1.6.1  1996/04/12  16:01:54  emcmanus
 *  Copied from mainline.ppc.
 *  [1996/04/12  15:53:36  emcmanus]
 *
 * Revision 1.1.4.1  1995/11/23  17:32:52  stephen
 *  first powerpc checkin to mainline.ppc
 *  [1995/11/23  17:14:20  stephen]
 *
 * Revision 1.1.2.1  1995/08/25  07:40:33  stephen
 *  Initial checkin
 *  [1995/08/25  07:29:02  stephen]
 *
 *  Initial checkin of files for PowerPC port
 *  [1995/08/23  15:28:26  stephen]
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

#ifndef PACK_MESSAGES
#define PACK_MESSAGES TRUE
#endif
