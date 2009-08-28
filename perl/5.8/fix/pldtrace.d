
/* Perl dtrace provider */

provider perl {
    probe sub__entry(char *, char *, int);
    probe sub__return(char *, char *, int);
};

/*

Entry
	1. char * (sub name, GvENAME(CvGV(cv)))
	2. char * (file name, CopFILE((COP*)CvSTART(cv)))
	3. int    (line number, CopLINE((COP*)CvSTART(cv)))

Return
	1. char * (sub name, GvENAME(CvGV(cv)))
	2. char * (file name, CopFILE((COP*)CvSTART(cv)))
	3. int    (line number, CopLINE((COP*)CvSTART(cv)))

*/



/*
  The definitions for these below are here:
	http://docs.sun.com/app/docs/doc/817-6223/6mlkidlnp?a=view
*/
#pragma D attributes unstable/unstable/Common provider perl provider
#pragma D attributes unstable/unstable/Common provider perl module
#pragma D attributes unstable/unstable/Common provider perl function
#pragma D attributes unstable/unstable/Common provider perl name
#pragma D attributes unstable/unstable/Common provider perl args
