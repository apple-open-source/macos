
/* $Id: kcglue_des.h,v 1.1 2002/02/28 00:23:25 snsimon Exp $
 * kclient and des have different definitions for key schedules
 * this file is to include in the kclient code without dragging in the des definitions
 */
int kcglue_des_key_sched(void *akey,void *asched);
void kcglue_des_ecb_encrypt(void *asrc,void *adest,void *asched,int direction);
void kcglue_des_pcbc_encrypt(void *asrc,void *adest,long length,void *asched,void *akey,int direction);
