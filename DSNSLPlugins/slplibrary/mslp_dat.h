/*
 * mslp_dat.h : Minimal SLP v2 DATable definitions.
 *
 * Version: 1.7
 * Date:    03/29/99
 *
 * Licensee will, at its expense,  defend and indemnify Sun Microsystems,
 * Inc.  ("Sun")  and  its  licensors  from  and  against any third party
 * claims, including costs and reasonable attorneys' fees,  and be wholly
 * responsible for  any liabilities  arising  out  of  or  related to the
 * Licensee's use of the Software or Modifications.   The Software is not
 * designed  or intended for use in  on-line  control  of  aircraft,  air
 * traffic,  aircraft navigation,  or aircraft communications;  or in the
 * design, construction, operation or maintenance of any nuclear facility
 * and Sun disclaims any express or implied warranty of fitness  for such
 * uses.  THE SOFTWARE IS PROVIDED TO LICENSEE "AS IS" AND ALL EXPRESS OR
 * IMPLIED CONDITION AND WARRANTIES, INCLUDING  ANY  IMPLIED  WARRANTY OF
 * MERCHANTABILITY,   FITNESS  FOR  WARRANTIES,   INCLUDING  ANY  IMPLIED
 * WARRANTY  OF  MERCHANTABILITY,  FITNESS FOR PARTICULAR PURPOSE OR NON-
 * INFRINGEMENT, ARE DISCLAIMED. IN NO EVENT WILL SUN BE LIABLE HEREUNDER
 * FOR ANY DIRECT DAMAGES OR ANY INDIRECT, PUNITIVE, SPECIAL, INCIDENTAL
 * OR CONSEQUENTIAL DAMAGES OF ANY KIND.
 *
 * (c) Sun Microsystems, 1998, All Rights Reserved.
 * Author: Erik Guttman
 */

#ifndef __DATABLE__
#define __DATABLE__

#define DATINCR 20
#define LISTINCR 256
#define kNumberOfStrikesAllowed 3

typedef struct daentry {
    
    struct sockaddr_in	sin;
    char				*pcScopeList;
    long				lBootTime;
    int					iStrikes;
    bool				iDAIsScopeSponser;		// this DA told us what scope to register in
} DAEntry;

typedef struct datable {

  DAEntry *pDAE;
  int      iMax;  /* how big can the table be */
  int      iSize; /* how many do we have now  */
  int      initialized; /* whether the da table has been initialized */
  
#ifdef EXTRA_MSGS
  char    *pcSASList;
  int      iSASListSz;
#endif /* EXTRA_MSGS */

} DATable; 

#ifdef	__cplusplus
//extern "C" {
#endif


extern EXPORT DATable  *dat_init();
extern EXPORT void      dat_delete(DATable *);
extern EXPORT int       dat_daadvert_in(DATable *, struct sockaddr_in, const char *, long);
extern EXPORT SLPInternalError  dat_get_da(const DATable *,const char *, struct sockaddr_in *);
extern EXPORT void		dat_boot_off_struck_out_das( void );
extern EXPORT SLPInternalError  dat_strike_da(DATable *, struct sockaddr_in);
extern EXPORT SLPInternalError	dat_update_da_scope_sponser_info(struct sockaddr_in sin, bool isScopeSponser);
extern EXPORT SLPInternalError  dat_get_scopes(SLPHandle slph, const char *pc, const DATable *, char **);

#ifdef EXTRA_MSGS
extern EXPORT void      dat_saadvert_in(DATable *, const char *, const char *);
extern EXPORT SLPInternalError  active_sa_discovery(SLPHandle slph, const char *pcHint);
#ifdef MAC_OS_X
extern EXPORT SLPInternalError  active_sa_async_discovery(SLPHandle hSLP, SLPScopeCallback callback, void *pvUser, const char *pcTypeHint);
#endif
#endif /* EXTRA_MSGS */

#ifdef	__cplusplus
//}
#endif

#endif /* __DATABLE__ */








