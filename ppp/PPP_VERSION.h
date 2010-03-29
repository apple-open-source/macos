/* 
    This file contains the version number of the ppp package.
    it follows the cvs tag, allow easy tracking ppp sources and
    must be changed before every tagging and submission 

*/


/* Current release of ppp, MUST be changed before submission */
#define CURRENT_RELEASE_TAG		"412.0.10"

/* Current working tag */
#define CURRENT_DEVELOPMENT_TAG		"3468584"


#if (!defined(DEVELOPMENT))

/* Development version of ppp */
#define PPP_VERSION		CURRENT_RELEASE_TAG " [Engineering build " CURRENT_DEVELOPMENT_TAG ", " __DATE__ " " __TIME__ "]"

#else

/* Release version pf ppp */
#define PPP_VERSION		CURRENT_RELEASE_TAG

#endif
