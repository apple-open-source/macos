# bufInt.decls --
#
#	This file contains the declarations for all unsupported
#	functions that are exported by the Buf library.  This file
#	is used to generate the bufIntDecls.h file
#

library buf

# Define the unsupported generic interfaces.

interface bufInt

# Declare each of the functions in the unsupported internal Buf
# interface.  These interfaces are allowed to change between versions.
# Use at your own risk.  Note that the position of functions should not
# be changed between versions to avoid gratuitous incompatibilities.

