#include <stdarg.h>
#include "stuff/ofile.h"
#include "stuff/print.h"
#include "stuff/errors.h"

__private_extern__
void
archive_error(
struct ofile *ofile,
const char *format, ...)
{
    va_list ap;

	va_start(ap, format);
	if(ofile->file_type == OFILE_FAT){
	    print("%s: for architecture %s archive: %s ",
		  progname, ofile->arch_flag.name, ofile->file_name);
	}
	else{
	    print("%s: archive: %s ", progname, ofile->file_name);
	}
	vprint(format, ap);
        print("\n");
	va_end(ap);
	errors++;
}

__private_extern__
void
archive_member_error(
struct ofile *ofile,
const char *format, ...)
{
    va_list ap;

	va_start(ap, format);
	if(ofile->file_type == OFILE_FAT){
	    print("%s: for architecture %s archive member: %s(%.*s) ",
		  progname, ofile->arch_flag.name, ofile->file_name,
		  (int)ofile->member_name_size, ofile->member_name);
	}
	else{
	    print("%s: archive member: %s(%.*s) ", progname, ofile->file_name,
		  (int)ofile->member_name_size, ofile->member_name);
	}
	vprint(format, ap);
        print("\n");
	va_end(ap);
	errors++;
}

#ifndef OTOOL
__private_extern__
void
Mach_O_error(
struct ofile *ofile,
const char *format, ...)
{
    va_list ap;

	va_start(ap, format);
	if(ofile->file_type == OFILE_FAT){
	    if(ofile->arch_type == OFILE_ARCHIVE){
		print("%s: for architecture %s object: %s(%.*s) ", progname,
		      ofile->arch_flag.name, ofile->file_name,
		      (int)ofile->member_name_size, ofile->member_name);
	    }
	    else{
		print("%s: for architecture %s object: %s ", progname,
		      ofile->arch_flag.name, ofile->file_name);
	    }
	}
	else if(ofile->file_type == OFILE_ARCHIVE){
	    if(ofile->member_type == OFILE_FAT){
		print("%s: for object: %s(%.*s) architecture %s ", progname,
		      ofile->file_name, (int)ofile->member_name_size,
		      ofile->arch_flag.name, ofile->member_name);
	    }
	    else{
		print("%s: object: %s(%.*s) ", progname, ofile->file_name,
		      (int)ofile->member_name_size, ofile->member_name);
	    }
	}
	else{
	    print("%s: object: %s ", progname, ofile->file_name);
	}
	vprint(format, ap);
        print("\n");
	va_end(ap);
	errors++;
}
#endif /* !defined(OTOOL) */
