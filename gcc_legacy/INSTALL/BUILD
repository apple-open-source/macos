
                               Building egcs
                                      
   Now that egcs is configured, you are ready to build the compiler and
   runtime libraries.
   
   We _highly_ recommend that egcs be built using gnu-make; other
   versions may work, then again they might not. To be safe build with
   gnu-make.
   
   _Building a native compiler_
   
   For a native build issue the command "make bootstrap". This will build
   the entire egcs compiler system, which includes the following steps:
     * Build host tools necessary to build the compiler such as texinfo,
       bison, gperf.
     * Build target tools for use by the compiler such as gas, gld, and
       binutils if they have been properly unpacked into the egcs source
       tree.
     * Perform a 3-stage bootstrap of the compiler.
     * Perform a comparison test of the stage2 and stage3 compilers.
     * Build runtime libraries using the stage3 compiler from the
       previous step.
       
   If you are short on disk space you might consider "make
   bootstrap-lean" instead. This is identical to "make bootstrap" except
   that object files from the stage1 and stage2 of the 3-stage bootstrap
   of the compiler are deleted as soon as they are no longer needed.
   
   If you want to save additional space during the bootstrap and in the
   final installation as well, you can build the compiler binaries
   without debugging information with "make CFLAGS='-O' LIBCFLAGS='-g
   -O2' LIBCXXFLAGS='-g -O2 -fno-implicit-templates' bootstrap". This
   will save roughly 40% of disk space both for the bootstrap and the
   final installation. (Libraries will still contain debugging
   information.)
   
   If you used the flag --enable-languages=... to restrict the compilers
   to be built, only those you've actually enabled will be built. This
   will of course only build those runtime libraries, for which the
   particular compiler has been built. Please note, that re-defining
   LANGUAGES when calling make bootstrap _*does not*_ work anymore!
   
   _Building a cross compiler_
   
   We recommend reading the [1]crossgcc FAQ for information about
   building cross compilers.
   
   For a cross build, issue the command "make cross", which performs the
   following steps:
     * Build host tools necessary to build the compiler such as texinfo,
       bison, gperf.
     * Build target tools for use by the compiler such as gas, gld, and
       binutils if they have been properly unpacked into the egcs source
       tree.
     * Build the compiler (single stage only).
     * Build runtime libraries using the compiler from the previous step.
       
   Note that if an error occurs in any step the make process will exit.
     _________________________________________________________________
   
   _Last modified on Dec 6, 1998._

References

   1. ftp://ftp.cygnus.com/pub/embedded/crossgcc/FAQ
