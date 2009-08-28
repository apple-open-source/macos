
#if __ppc__

    .text

_prejunk:
    mr  r3,r5
    mr  r3,r4
    blr


_space1:
    .space 15*1024*1024 + 2
    
    .align 5
_junk:
    mr  r3,r5
    mr  r3,r4
    blr
    
    
_space2:
    .space 2*1024*1024
 
#endif


#if __arm__


_space1:
    .space 32*1024*1024 + 2


#endif


    .subsections_via_symbols
    