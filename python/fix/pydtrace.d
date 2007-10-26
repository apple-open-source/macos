
/* Python dtrace provider */

provider python {
    probe entry(char *, char *, int, int);
    probe exit(char *, char *, char *);
};

/*

   f      ==> PyFrameObject from compile.h
   f_code ==> PyCodeObject  from frameobject.h

Entry
	1. char * (file name, f->f_code->co_filename->ob_sval)
	2. char * (function name, f->f_code->co_name->ob_sval)
	3. int    (line number, f->f_lineno)
	4. int	  (argument count, f->f_code->co_argcount)

Exit
	1. char * (file name, f->f_code->co_filename->ob_sval)
	2. char * (function name, f->f_code->co_name->ob_sval)
	3. char * (object type char * , object->ob_type->tp_name)

*/



/*
  The definitions for these below are here:
	http://docs.sun.com/app/docs/doc/817-6223/6mlkidlnp?a=view
*/
#pragma D attributes unstable/unstable/Common provider python provider
#pragma D attributes unstable/unstable/Common provider python module
#pragma D attributes unstable/unstable/Common provider python function
#pragma D attributes unstable/unstable/Common provider python name
#pragma D attributes unstable/unstable/Common provider python args
