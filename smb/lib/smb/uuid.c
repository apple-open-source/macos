/*
 * 
 * (c) Copyright 1989 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989 DIGITAL EQUIPMENT CORPORATION
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 *                 permission to use, copy, modify, and distribute this
 * file for any purpose is hereby granted without fee, provided that
 * the above copyright notices and this notice appears in all source
 * code copies, and that none of the names of Open Software
 * Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 * Corporation be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Neither Open Software Foundation, Inc., Hewlett-
 * Packard Company, nor Digital Equipment Corporation makes any
 * representations about the suitability of this software for any
 * purpose.
 * 
 */
/*
 */
/*
**
**  NAME:
**
**      uuid.c
**
**  FACILITY:
**
**      UUID
**
**  ABSTRACT:
**
**      UUID - routines that manipulate uuid's
**
**
*/

#include <commonp.h>            /* common definitions                   */
#include <dce/uuid.h>           /* uuid idl definitions (public)        */
#include <uuidp.h>              /* uuid idl definitions (private)       */


/***************************************************************************
 *
 * Local definitions
 *
 **************************************************************************/

#ifdef  UUID_DEBUG
#define DEBUG_PRINT(msg, st)    RPC_DBG_GPRINTF (( "%s: %08x\n", msg, st ))
#else
#define DEBUG_PRINT             { /* hello sailor */ }
#endif

#ifndef NO_SSCANF
#  define UUID_SSCANF          sscanf
#  define UUID_OLD_SSCANF      sscanf
#else
#  define UUID_SSCANF          uuid__sscanf
#  define UUID_OLD_SSCANF      uuid__old_sscanf
#endif

#ifndef NO_SPRINTF
#  define UUID_SPRINTF         sprintf
#  define UUID_OLD_SPRINTF     sprintf
#else
#  define UUID_SPRINTF         uuid__sprintf
#  define UUID_OLD_SPRINTF     uuid__old_sprintf
#endif

/*
 * the number of elements returned by sscanf() when converting
 * string formatted uuid's to binary
 */
#define UUID_ELEMENTS_NUM       11
#define UUID_ELEMENTS_NUM_OLD   10


/****************************************************************************
 *
 * global data declarations
 *
 ****************************************************************************/

GLOBAL uuid_t uuid_g_nil_uuid = { 0, 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } };
GLOBAL uuid_t uuid_nil = { 0, 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } };

/****************************************************************************
 *
 * local function declarations
 *
 ****************************************************************************/

/*
 * I N I T
 *
 * Startup initialization routine for UUID module.
 */

INTERNAL void init( unsigned32 * /*st*/ );


/*****************************************************************************
 *
 *  Macro definitions
 *
 ****************************************************************************/

/*
 * ensure we've been initialized
 */
INTERNAL boolean uuid_init_done = FALSE;

#define EmptyArg
#define UUID_VERIFY_INIT(Arg)          \
    if (! uuid_init_done)           \
    {                               \
        init (status);              \
        if (*status != uuid_s_ok)   \
        {                           \
            return Arg;                 \
        }                           \
    }

/*
 * Check the reserved bits to make sure the UUID is of the known structure.
 */

#define CHECK_STRUCTURE(uuid) \
( \
    (((uuid)->clock_seq_hi_and_reserved & 0x80) == 0x00) || /* var #0 */ \
    (((uuid)->clock_seq_hi_and_reserved & 0xc0) == 0x80) || /* var #1 */ \
    (((uuid)->clock_seq_hi_and_reserved & 0xe0) == 0xc0)    /* var #2 */ \
)

/*
 * The following macros invoke CHECK_STRUCTURE(), check that the return
 * value is okay and if not, they set the status variable appropriately
 * and return either a boolean FALSE, nothing (for void procedures),
 * or a value passed to the macro.  This has been done so that checking
 * can be done more simply and values are returned where appropriate
 * to keep compilers happy.
 *
 * bCHECK_STRUCTURE - returns boolean FALSE
 * vCHECK_STRUCTURE - returns nothing (void)
 * rCHECK_STRUCTURE - returns 'r' macro parameter
 */

#define bCHECK_STRUCTURE(uuid, status) \
{ \
    if (!CHECK_STRUCTURE (uuid)) \
    { \
        *(status) = uuid_s_bad_version; \
        return (FALSE); \
    } \
}

#define vCHECK_STRUCTURE(uuid, status) \
{ \
    if (!CHECK_STRUCTURE (uuid)) \
    { \
        *(status) = uuid_s_bad_version; \
        return; \
    } \
}

#define rCHECK_STRUCTURE(uuid, status, result) \
{ \
    if (!CHECK_STRUCTURE (uuid)) \
    { \
        *(status) = uuid_s_bad_version; \
        return (result); \
    } \
}

/*
**++
**
**  ROUTINE NAME:       init
**
**  SCOPE:              INTERNAL - declared locally
**
**  DESCRIPTION:
**
**  Startup initialization routine for the UUID module.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       sets uuid_init_done so this won't be done again
**
**--
**/

INTERNAL void init 
(
    unsigned32              *status
)
{
#ifdef CMA_INCLUDE
    /*
     * Hack for CMA pthreads.  Some users will call uuid_ stuff before
     * doing any thread stuff (CMA is uninitialized).  Some uuid_
     * operations alloc memory and in a CMA pthread environment,
     * the malloc wrapper uses a mutex but doesn't do self initialization
     * (which results in segfault'ing inside of CMA).  Make sure that
     * CMA is initialized.
     */
    pthread_t   t;
    t = pthread_self();
#endif /* CMA_INCLUDE */

    CODING_ERROR (status);

    uuid_init_done = TRUE;

    *status = uuid_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       uuid_create
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Create a new UUID. Note: we only know how to create the new
**  and improved UUIDs.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      uuid            A new UUID value
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void uuid_create 
(
    uuid_t                  *uuid,
    unsigned32              *status
)
{
    RPC_LOG_UUID_CREATE_NTR;

    CODING_ERROR (status);
    UUID_VERIFY_INIT (EmptyArg);

    /*
     * Call the system's UUID generator.
     */
    uuid__generate((void *)uuid);

    *status = uuid_s_ok;
    RPC_LOG_UUID_CREATE_XIT;
}

/*
**++
**
**  ROUTINE NAME:       uuid_create_nil
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Create a 'nil' uuid.
**
**  INPUTS:             none
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      uuid            A nil UUID
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void uuid_create_nil 
(
    uuid_t              *uuid,
    unsigned32          *status
)
{
    CODING_ERROR (status);
    UUID_VERIFY_INIT (EmptyArg);
    memset (uuid, 0, sizeof (uuid_t));

    *status = uuid_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       uuid_to_string
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Encode a UUID into a printable string.
**
**  INPUTS:
**
**      uuid            A binary UUID to be converted to a string UUID.
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      uuid_string     The string representation of the given UUID.
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_bad_version
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void uuid_to_string 
(
    uuid_p_t                uuid,
    unsigned_char_p_t       *uuid_string,
    unsigned32              *status
)
{

    CODING_ERROR (status);
    UUID_VERIFY_INIT (EmptyArg);

    /*
     * don't do anything if the output argument is NULL
     */
    if (uuid_string == NULL)
    {
        *status = uuid_s_ok;
        return;
    }

    vCHECK_STRUCTURE (uuid, status);

    RPC_MEM_ALLOC (
        *uuid_string,
        unsigned_char_p_t,
        UUID_C_UUID_STRING_MAX,
        RPC_C_MEM_STRING,
        RPC_C_MEM_WAITOK);

    if (*uuid_string == NULL)
    {
        *status = uuid_s_no_memory;
        return;
    }

    UUID_SPRINTF(
        (char *) *uuid_string,
        "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid->time_low, uuid->time_mid, uuid->time_hi_and_version,
        uuid->clock_seq_hi_and_reserved, uuid->clock_seq_low,
        (unsigned8) uuid->node[0], (unsigned8) uuid->node[1],
        (unsigned8) uuid->node[2], (unsigned8) uuid->node[3],
        (unsigned8) uuid->node[4], (unsigned8) uuid->node[5]);

    *status = uuid_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       uuid_from_string
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Decode a UUID from a printable string.
**
**  INPUTS:
**
**      uuid_string     The string UUID to be converted to a binary UUID
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      uuid            The binary representation of the given UUID
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_bad_version
**          uuid_s_invalid_string_uuid
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     void
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC void uuid_from_string 
(
    unsigned_char_p_t       uuid_string,
    uuid_t                  *uuid,
    unsigned32              *status
)
{
    uuid_t              uuid_new;       /* used for sscanf for new uuid's */
    uuid_old_t          uuid_old;       /* used for sscanf for old uuid's */
    uuid_p_t            uuid_ptr;       /* pointer to correct uuid (old/new) */
    int                 i;


    CODING_ERROR (status);
    UUID_VERIFY_INIT(EmptyArg);

    /*
     * If a NULL pointer or empty string, give the nil UUID.
     */
    if (uuid_string == NULL || *uuid_string == '\0')
    {
	memcpy (uuid, &uuid_g_nil_uuid, sizeof *uuid);
	*status = uuid_s_ok;
	return;
    }

    /*
     * check to see that the string length is right at least
     */
    if (strlen ((char *) uuid_string) != UUID_C_UUID_STRING_MAX - 1)
    {
        *status = uuid_s_invalid_string_uuid;
        return;
    }

    /*
     * check for a new uuid
     */
    if (uuid_string[8] == '-')
    {
        long    time_low;
        int     time_mid;
        int     time_hi_and_version;
        int     clock_seq_hi_and_reserved;
        int     clock_seq_low;
        int     node[6];


        i = UUID_SSCANF(
            (char *) uuid_string, "%8lx-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x",
            &time_low,
            &time_mid,
            &time_hi_and_version,
            &clock_seq_hi_and_reserved,
            &clock_seq_low,
            &node[0], &node[1], &node[2], &node[3], &node[4], &node[5]);

        /*
         * check that sscanf worked
         */
        if (i != UUID_ELEMENTS_NUM)
        {
            *status = uuid_s_invalid_string_uuid;
            return;
        }

        /*
         * note that we're going through this agony because scanf is defined to
         * know only to scan into "int"s or "long"s.
         */
        uuid_new.time_low                   = time_low;
        uuid_new.time_mid                   = time_mid;
        uuid_new.time_hi_and_version        = time_hi_and_version;
        uuid_new.clock_seq_hi_and_reserved  = clock_seq_hi_and_reserved;
        uuid_new.clock_seq_low              = clock_seq_low;
        uuid_new.node[0]                    = node[0];
        uuid_new.node[1]                    = node[1];
        uuid_new.node[2]                    = node[2];
        uuid_new.node[3]                    = node[3];
        uuid_new.node[4]                    = node[4];
        uuid_new.node[5]                    = node[5];

        /*
         * point to the correct uuid
         */
        uuid_ptr = &uuid_new;
    }
    else
    {
        long    time_high;
        int     time_low;
        int     family;
        int     host[7];


        /*
         * format = tttttttttttt.ff.h1.h2.h3.h4.h5.h6.h7
         */
        i = UUID_OLD_SSCANF(
            (char *) uuid_string, "%8lx%4x.%2x.%2x.%2x.%2x.%2x.%2x.%2x.%2x",
            &time_high, &time_low, &family,
            &host[0], &host[1], &host[2], &host[3],
            &host[4], &host[5], &host[6]);

        /*
         * check that sscanf worked
         */
        if (i != UUID_ELEMENTS_NUM_OLD)
        {
            *status = uuid_s_invalid_string_uuid;
            return;
        }

        /*
         * note that we're going through this agony because scanf is defined to
         * know only to scan into "int"s or "long"s.
         */
        uuid_old.time_high      = time_high;
        uuid_old.time_low       = time_low;
        uuid_old.family         = family;
        uuid_old.host[0]        = host[0];
        uuid_old.host[1]        = host[1];
        uuid_old.host[2]        = host[2];
        uuid_old.host[3]        = host[3];
        uuid_old.host[4]        = host[4];
        uuid_old.host[5]        = host[5];
        uuid_old.host[6]        = host[6];

        /*
         * fix up non-string field, and point to the correct uuid
         */
        uuid_old.reserved = 0;
        uuid_ptr = (uuid_p_t) (&uuid_old);
    }

    /*
     * sanity check, is version ok?
     */
    vCHECK_STRUCTURE (uuid_ptr, status);

    /*
     * copy the uuid to user
     */
    memcpy (uuid, uuid_ptr, sizeof (uuid_t));

    *status = uuid_s_ok;
}

/*
**++
**
**  ROUTINE NAME:       uuid_equal
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Compare two UUIDs.
**
**  INPUTS:
**
**      uuid1           The first UUID to compare
**
**      uuid2           The second UUID to compare
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_bad_version
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      result          true if UUID's are equal
**                      false if UUID's are not equal
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC boolean32 uuid_equal 
(
    register uuid_p_t                uuid1,
    register uuid_p_t                uuid2,
    register unsigned32              *status
)
{
    RPC_LOG_UUID_EQUAL_NTR;
    CODING_ERROR (status);
    UUID_VERIFY_INIT (FALSE);

    bCHECK_STRUCTURE (uuid1, status);
    bCHECK_STRUCTURE (uuid2, status);

    *status = uuid_s_ok;

    /*
     * Note: This used to be a memcmp(), but changed to a field-by-field compare
     * because of portability problems with alignment and garbage in a UUID.
     */
    if ((uuid1->time_low == uuid2->time_low) && 
	(uuid1->time_mid == uuid2->time_mid) &&
	(uuid1->time_hi_and_version == uuid2->time_hi_and_version) && 
	(uuid1->clock_seq_hi_and_reserved == uuid2->clock_seq_hi_and_reserved) &&
	(uuid1->clock_seq_low == uuid2->clock_seq_low) &&
	(memcmp(uuid1->node, uuid2->node, 6) == 0))
    {
	RPC_LOG_UUID_EQUAL_XIT;
	return ( TRUE );
    }
    else
    {
        RPC_LOG_UUID_EQUAL_XIT;
        return (FALSE);
    }
}

/*
**++
**
**  ROUTINE NAME:       uuid_is_nil
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Check to see if a given UUID is 'nil'.
**
**  INPUTS:             none
**
**      uuid            A UUID
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_bad_version
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      result          true if UUID is nil
**                      false if UUID is not nil
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC boolean32 uuid_is_nil 
(
    uuid_p_t            uuid,
    unsigned32          *status
)
{
    CODING_ERROR (status);
    UUID_VERIFY_INIT (FALSE);

    bCHECK_STRUCTURE (uuid, status);

    *status = uuid_s_ok;

    /*
     * Note: This should later be changed to a field-by-field compare
     * because of portability problems with alignment and garbage in a UUID.
     */
    if (memcmp (uuid, &uuid_g_nil_uuid, sizeof (uuid_t)) == 0)
    {
        return (TRUE);
    }
    else
    {
        return (FALSE);
    }
}

/*
**++
**
**  ROUTINE NAME:       uuid_compare
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Compare two UUID's "lexically"
**
**  If either of the two arguments is given as a NULL pointer, the other
**  argument will be compared to the nil uuid.
**
**  Note:   1) lexical ordering is not temporal ordering!
**          2) in the interest of keeping this routine short, I have
**             violated the coding convention that says all if/else
**             constructs shall have {}'s. There are a little million
**             return()'s in this routine. FWIW, the only {}'s that
**             are really required are the ones in the for() loop.
**
**  INPUTS:
**
**      uuid1           The first UUID to compare
**
**      uuid2           The second UUID to compare
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_bad_version
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:     uuid_order_t
**
**      -1   uuid1 is lexically before uuid2
**      1    uuid1 is lexically after uuid2
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC signed32 uuid_compare 
(
    uuid_p_t                uuid1,
    uuid_p_t                uuid2,
    unsigned32              *status
)
{
    int                 i;

    CODING_ERROR (status);
    UUID_VERIFY_INIT (FALSE);


    /*
     * check to see if either of the arguments is a NULL pointer
     * - if so, compare the other argument to the nil uuid
     */
    if (uuid1 == NULL)
    {
        /*
         * if both arguments are NULL, so is this routine
         */
        if (uuid2 == NULL)
        {
            *status = uuid_s_ok;
            return (0);
        }

        rCHECK_STRUCTURE (uuid2, status, -1);
        return (uuid_is_nil (uuid2, status) ? 0 : -1);
    }

    if (uuid2 == NULL)
    {
        rCHECK_STRUCTURE (uuid1, status, -1);
        return (uuid_is_nil (uuid1, status) ? 0 : 1);
    }

    rCHECK_STRUCTURE (uuid1, status, -1);
    rCHECK_STRUCTURE (uuid2, status, -1);

    *status = uuid_s_ok;

    if (uuid1->time_low == uuid2->time_low)
    {
        if (uuid1->time_mid == uuid2->time_mid)
        {
            if (uuid1->time_hi_and_version == uuid2->time_hi_and_version)
            {
                if (uuid1->clock_seq_hi_and_reserved
                    == uuid2->clock_seq_hi_and_reserved)
                {
                    if (uuid1->clock_seq_low == uuid2->clock_seq_low)
                    {
                        for (i = 0; i < 6; i++)
                        {
                            if (uuid1->node[i] < uuid2->node[i])
                                return (-1);
                            if (uuid1->node[i] > uuid2->node[i])
                                return (1);
                        }
                        return (0);
                    }       /* end if - clock_seq_low */
                    else
                    {
                        if (uuid1->clock_seq_low < uuid2->clock_seq_low)
                            return (-1);
                        else
                            return (1);
                    }       /* end else - clock_seq_low */
                }           /* end if - clock_seq_hi_and_reserved */
                else
                {
                    if (uuid1->clock_seq_hi_and_reserved
                        < uuid2->clock_seq_hi_and_reserved)
                        return (-1);
                    else
                        return (1);
                }           /* end else - clock_seq_hi_and_reserved */
            }               /* end if - time_hi_and_version */
            else
            {
                if (uuid1->time_hi_and_version < uuid2->time_hi_and_version)
                    return (-1);
                else
                    return (1);
            }               /* end else - time_hi_and_version */
        }                   /* end if - time_mid */
        else
        {
            if (uuid1->time_mid < uuid2->time_mid)
                return (-1);
            else
                return (1);
        }                   /* end else - time_mid */
    }                       /* end if - time_low */
    else
    {
        if (uuid1->time_low < uuid2->time_low)
            return (-1);
        else
            return (1);
    }                       /* end else - time_low */
}

/*
**++
**
**  ROUTINE NAME:       uuid_hash
**
**  SCOPE:              PUBLIC - declared in UUID.IDL
**
**  DESCRIPTION:
**
**  Return a hash value for a given UUID.
**
**  Note: Since the length of a UUID is architecturally defined to be
**        128 bits (16 bytes), we have forgone using a '#defined'
**        length.  In particular, since the 'loop' has been unrolled
**        (for performance) the length is by definition 'hard-coded'.
**
**  INPUTS:
**
**      uuid            A UUID for which a hash value is to be computed
**
**  INPUTS/OUTPUTS:     none
**
**  OUTPUTS:
**
**      status          return status value
**
**          uuid_s_ok
**          uuid_s_bad_version
**          uuid_s_coding_error
**
**  IMPLICIT INPUTS:    none
**
**  IMPLICIT OUTPUTS:   none
**
**  FUNCTION VALUE:
**
**      hash_value      The hash value computed from the UUID
**
**  SIDE EFFECTS:       none
**
**--
**/

PUBLIC unsigned16 uuid_hash 
(
    uuid_p_t                uuid,
    unsigned32              *status
)
{
    short               c0, c1;
    short               x, y;
    byte_p_t            next_uuid;

    RPC_LOG_UUID_HASH_NTR;

    CODING_ERROR (status);
    UUID_VERIFY_INIT (FALSE);

    rCHECK_STRUCTURE (uuid, status, 0);

    /*
     * initialize counters
     */
    c0 = c1 = 0;
    next_uuid = (byte_p_t) uuid;

    /*
     * For speed lets unroll the following loop:
     *
     *   for (i = 0; i < UUID_K_LENGTH; i++)
     *   {
     *       c0 = c0 + *next_uuid++;
     *       c1 = c1 + c0;
     *   }
     */
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;

    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;

    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;

    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;

    /*
     *  Calculate the value for "First octet" of the hash
     */
    x = -c1 % 255;
    if (x < 0)
    {
        x = x + 255;
    }

    /*
     *  Calculate the value for "second octet" of the hash
     */
    y = (c1 - c0) % 255;
    if (y < 0)
    {
        y = y + 255;
    }

    /*
     * return the pieces put together
     */
    *status = uuid_s_ok;

    RPC_LOG_UUID_HASH_XIT;
    return ((y * 256) + x);
}
