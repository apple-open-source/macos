# buf.decls -- -*- tcl -*-
#
#	This file contains the declarations for all supported public
#	functions that are exported by the Buf library inside of
#	memchan via its stub table.
#
#	This file is used to generate the bufDecls.h file.
#	

library buf

# Define the buf interface with several sub interfaces:
#     bufPlat	 - platform specific public
#     bufInt	 - generic private
#     bufPlatInt - platform specific private

interface buf
hooks {bufInt memchan}

# Declare each of the functions in the public Buf interface.  Note that
# every index should never be reused for a different function in order
# to preserve backwards compatibility.

# --------------------------------------------------
# Initialization of buf ... Already done ?, Do it.

declare 0 generic {
    int Buf_IsInitialized (Tcl_Interp *interp)
}

declare 1 generic {
    int Buf_Init (Tcl_Interp *interp)
}

# --------------------------------------------------
# Management of buffer types.

declare 10 generic {
    void Buf_RegisterType (Buf_BufferType* bufType)
}

# --------------------------------------------------
# Refcount management, general accessors

declare 20 generic {
    void Buf_IncrRefcount (Buf_Buffer buf)
}

declare 21 generic {
    void Buf_DecrRefcount (Buf_Buffer buf)
}

declare 22 generic {
    int Buf_IsShared (Buf_Buffer buf)
}

declare 23 generic {
    Buf_BufferType* Buf_GetType (Buf_Buffer buf)
}

declare 24 generic {
    CONST char* Buf_GetTypeName (Buf_Buffer buf)
}

declare 25 generic {
    int Buf_Size (Buf_Buffer buf)
}

declare 26 generic {
    ClientData Buf_GetClientData (Buf_Buffer buf)
}



# --------------------------------------------------
# Creation of buffers (generic, for the predefined types).

# Generic creation of a new buffer from clientdata for a
# specific type and the type reference itself.

declare 30 generic {
    Buf_Buffer Buf_Create (Buf_BufferType* bufType, ClientData clientData)
}

# Create an independent duplicate of the incoming buffer.
# (May defer the actual duplication of the content until
# the first write access to any of the copies!)

declare 31 generic {
    Buf_Buffer Buf_Dup (Buf_Buffer buf)
}

# --------------------------------------------------
# Create a buffer of fixed size, containing at most
# size bytes.

declare 32 generic {
    Buf_Buffer Buf_CreateFixedBuffer (int size)
}

# Create an extendable buffer with an initial size of
# size bytes.

declare 33 generic {
    Buf_Buffer Buf_CreateExtendableBuffer (int size)
}

# Create a buffer which is a fixed range in another
# buffer. It contains 'size' bytes and starts at the
# current read position of the underlying buffer.
# The range references the underlying buffer and
# consequently increments its refcount.
#
# Creating a range from a range is possible, but is
# also automatically reduced to a range over a non-
# range.
#
# The range is not allowed to exceed the current size
# of the underlying buffer.

declare 34 generic {
    Buf_Buffer Buf_CreateRange (Buf_Buffer buf, int size)
}

# --------------------------------------------------
# Reading and writing data from/to a buffer, positions

# Read at most size bytes from the buffer into the array outbuf.
# Returns the number of read bytes.

declare 40 generic {
    int Buf_Read (Buf_Buffer buf, void* outbuf, int size)
}

# Take at most size bytes from the array inbuf and append it to the
# buffer. Returns the number of written bytes. Behaviour when writing
# to a range depends on the way the range was created.  A duplicate of
# a range or a range with duplicates lying aroung will now enforce
# actual separation of content. A normal range (without duplicates)
# will simply write into the underlying buffer. Creating separate
# ranges over the same area of a buffer creates dependent
# copies. Writing into one will affect all others.

declare 41 generic {
    int Buf_Write (Buf_Buffer buf, CONST void* inbuf, int size)
}

# --------------------------------------------------
# Buffer locations.

# Convert a location in the buffer into a pointer to the data at that
# location.

declare 50 generic {
    char* Buf_PositionPtr (Buf_BufferPosition loc)
}

# Ask a buffer for the current location to read from.

declare 51 generic {
    Buf_BufferPosition Buf_Tell (Buf_Buffer buf)
}

declare 52 generic {
    void Buf_FreePosition (Buf_BufferPosition loc)
}

declare 53 generic {
    void Buf_MovePosition (Buf_BufferPosition loc, int offset)
}

declare 54 generic {
    Buf_BufferPosition Buf_DupPosition (Buf_BufferPosition loc)
}

declare 55 generic {
    int Buf_PositionOffset (Buf_BufferPosition loc)
}

declare 56 generic {
    Buf_BufferPosition Buf_PositionFromOffset (Buf_Buffer buf, int offset)
}

# --------------------------------------------------
# Queues made out of buffers (Used fixed size buffers naturally).
# Read / Write are like on buffers, but extended.

declare 60 generic {
    Buf_BufferQueue Buf_NewQueue (void)
}

declare 61 generic {
    void Buf_FreeQueue (Buf_BufferQueue queue)
}

declare 62 generic {
    int Buf_QueueRead (Buf_BufferQueue queue, char* outbuf, int size)
}

declare 63 generic {
    int Buf_QueueWrite (Buf_BufferQueue queue, CONST char* inbuf, int size)
}

declare 64 generic {
    void Buf_QueueAppend (Buf_BufferQueue queue, Buf_Buffer buf)
}

declare 65 generic {
    int Buf_QueueSize (Buf_BufferQueue queue)
}


# --------------------------------------------------
# Place for more

# --------------------------------------------------
