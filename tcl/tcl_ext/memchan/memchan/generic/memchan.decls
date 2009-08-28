# memchan.decls -- Copyright (C) 2004 Pat Thoyts <patthoyts@users.sourceforge.net>
#
#	This file contains the declarations for all supported public
#	functions that are exported by the Memcgan library via its stubs table.
#
#	This file is used to generate the memchanDecls.h and memchanStubInit.c
#	
#	DO NOT RE-NUMBER THE FUNCTIONS.
#	To preserve backwards compatability, new functions MUST BE appended with a 
#	new unique number.
#
# $Id: memchan.decls,v 1.1 2004/11/09 23:11:00 patthoyts Exp $

library memchan
interface memchan
#hooks {}

declare 0 generic {
    int Memchan_Init (Tcl_Interp *interp)
}

declare 1 generic {
    int Memchan_SafeInit (Tcl_Interp *interp)
}

declare 2 generic {
    Tcl_Channel Memchan_CreateMemoryChannel(Tcl_Interp *interp, int initialSize)
}

declare 3 generic {
    Tcl_Channel Memchan_CreateFifoChannel(Tcl_Interp *interp)
}

declare 4 generic {
    void Memchan_CreateFifo2Channel(Tcl_Interp *interp, \
                                        Tcl_Channel *aPtr, Tcl_Channel *bPtr)
}

declare 5 generic {
    Tcl_Channel Memchan_CreateZeroChannel(Tcl_Interp *interp)
}

declare 6 generic {
    Tcl_Channel Memchan_CreateNullChannel(Tcl_Interp *interp)
}

declare 7 generic {
    Tcl_Channel Memchan_CreateRandomChannel(Tcl_Interp *interp)
}

# -------------------------------------------------------------------------
# Local variables:
# mode: tcl
# End:
