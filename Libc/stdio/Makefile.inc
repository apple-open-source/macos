#	@(#)Makefile.inc	8.3 (Berkeley) 4/17/94
# $FreeBSD: src/lib/libc/stdio/Makefile.inc,v 1.20 2001/03/27 17:26:52 ru Exp $

# stdio sources
.PATH: ${.CURDIR}/stdio

SRCS+=  clrerr.c findfp.c  fsetpos.c    mktemp.c     scanf.c      ungetc.c \
	fclose.c flags.c   ftell.c      perror.c     setbuf.c     vfprintf.c \
	fdopen.c fopen.c   funopen.c    printf.c     setbuffer.c  vfscanf.c \
	feof.c   fprintf.c fvwrite.c    putc.c       setvbuf.c    vprintf.c \
	ferror.c fpurge.c  fwalk.c      putchar.c    snprintf.c   vscanf.c \
	fflush.c fputc.c   fwrite.c     puts.c       sprintf.c    vsnprintf.c \
	fgetc.c  fputs.c   getc.c       putw.c       sscanf.c     vsprintf.c \
	fgetln.c fread.c   getchar.c    refill.c     stdio.c      vsscanf.c \
	fgetpos.c freopen.c gets.c       remove.c     tempnam.c    wbuf.c \
	fgets.c  fscanf.c  getw.c       rewind.c     tmpfile.c    wsetup.c \
	fileno.c fseek.c   makebuf.c    rget.c       tmpnam.c     asprintf.c \
	vasprintf.c


.if ${LIB} == "c"
MAN3+=	fclose.3 ferror.3 fflush.3 fgetln.3 fgets.3 fopen.3 fputs.3 \
	fread.3 fseek.3 funopen.3 getc.3 mktemp.3 printf.3 putc.3 remove.3 \
	scanf.3 setbuf.3 stdio.3 tmpnam.3 ungetc.3

MLINKS+=ferror.3 clearerr.3 ferror.3 feof.3 ferror.3 fileno.3
MLINKS+=fflush.3 fpurge.3
MLINKS+=fgets.3 gets.3
MLINKS+=fopen.3 fdopen.3 fopen.3 freopen.3
MLINKS+=fputs.3 puts.3
MLINKS+=fread.3 fwrite.3
MLINKS+=fseek.3 fgetpos.3 fseek.3 fseeko.3 fseek.3 fsetpos.3 fseek.3 ftell.3 \
	fseek.3 ftello.3 fseek.3 rewind.3
MLINKS+=funopen.3 fropen.3 funopen.3 fwopen.3
MLINKS+=getc.3 fgetc.3 getc.3 getchar.3 getc.3 getw.3
MLINKS+=mktemp.3 mkdtemp.3 mktemp.3 mkstemp.3 mktemp.3 mkstemps.3
MLINKS+=printf.3 asprintf.3 printf.3 fprintf.3 \
	printf.3 snprintf.3 printf.3 sprintf.3 \
	printf.3 vasprintf.3 \
	printf.3 vfprintf.3 printf.3 vprintf.3 printf.3 vsnprintf.3 \
	printf.3 vsprintf.3
MLINKS+=putc.3 fputc.3 putc.3 putchar.3 putc.3 putw.3
MLINKS+=scanf.3 fscanf.3 scanf.3 sscanf.3 scanf.3 vfscanf.3 scanf.3 vscanf.3 \
	scanf.3 vsscanf.3
MLINKS+=setbuf.3 setbuffer.3 setbuf.3 setlinebuf.3 setbuf.3 setvbuf.3
MLINKS+=tmpnam.3 tempnam.3 tmpnam.3 tmpfile.3
.endif
