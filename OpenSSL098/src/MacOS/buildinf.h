#ifndef MK1MF_BUILD
#  define DATE      __DATE__
# if   defined(__i386) || defined(__i386__) 
#  define CFLAGS    "-arch i386 -fmessage-length=0 -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -O3 -D_REENTRANT -DDSO_DLFCN -DHAVE_DLFCN_H -DL_ENDIAN -DOPENSSL_NO_IDEA -DOPENSSL_PIC -DOPENSSL_THREADS -DZLIB -mmacosx-version-min=10.6"
#  define PLATFORM  "darwin-i386-llvm"
# elif defined(__x86_64) || defined(__x86_64__)
#  define CFLAGS    "-arch x86_64 -fmessage-length=0 -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -O3 -D_REENTRANT -DDSO_DLFCN -DHAVE_DLFCN_H -DL_ENDIAN -DMD32_REG_T=int -DOPENSSL_NO_IDEA -DOPENSSL_PIC -DOPENSSL_THREADS -DZLIB -mmacosx-version-min=10.6"
#  define PLATFORM  "darwin64-x86_64-llvm"
# elif defined(__powerpc) || defined(__ppc__)
#  define CFLAGS    "-arch ppc -fmessage-length=0 -pipe -Wno-trigraphs -fpascal-strings -fasm-blocks -O3 -D_REENTRANT -DB_ENDIAN -DDSO_DLFCN -DHAVE_DLFCN_H -DOPENSSL_NO_IDEA -DOPENSSL_PIC -DOPENSSL_THREADS -DZLIB -mtune=G4 -mmacosx-version-min=10.6"
#  define PLATFORM  "darwin-ppc-llvm"
# endif
#endif
