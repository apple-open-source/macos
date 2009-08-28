# trf.decls --
#
#	This file contains the declarations for all supported public
#	functions that are exported by the Trf library via the stubs table.
#	This file is used to generate the trfDecls.h file.
#	

library trf

# Define the tcl interface with several sub interfaces:
#     tclPlat	 - platform specific public
#     tclInt	 - generic private
#     tclPlatInt - platform specific private

interface trf
hooks {trfInt}

# Declare each of the functions in the public Trf interface.  Note that
# every index should never be reused for a different function in order
# to preserve backwards compatibility.

declare 0 generic {
    int Trf_IsInitialized(Tcl_Interp *interp)
}
declare 1 generic {
    int Trf_Register(Tcl_Interp *interp, CONST Trf_TypeDefinition *type)
}
declare 2 generic {
    Trf_OptionVectors* Trf_ConverterOptions(void)
}
declare 3 generic {
    int Trf_LoadLibrary(Tcl_Interp *interp, CONST char *libName,
	    VOID **handlePtr, char **symbols, int num)
}
declare 4 generic {
    void Trf_LoadFailed(VOID** handlePtr)
}
declare 5 generic {
    int Trf_RegisterMessageDigest (Tcl_Interp* interp, CONST Trf_MessageDigestDescription* md_desc)
}
declare 6 generic {
    void Trf_XorBuffer (VOID* buffer, VOID* mask, int length)
}
declare 7 generic {
    void Trf_ShiftRegister (VOID* buffer, VOID* in, int shift, int buffer_length)
}
declare 8 generic {
    void Trf_FlipRegisterLong (VOID* buffer, int length)
}
declare 9 generic {
    void Trf_FlipRegisterShort (VOID* buffer, int length)
}
