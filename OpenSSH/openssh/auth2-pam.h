/* $Id: auth2-pam.h,v 1.1.1.1 2001/02/25 20:54:08 zarzycki Exp $ */

#include "includes.h"
#ifdef USE_PAM

int	auth2_pam(Authctxt *authctxt);

#endif /* USE_PAM */
