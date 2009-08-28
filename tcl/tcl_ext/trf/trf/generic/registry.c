/*
 * registry.c --
 *
 *	Implements the C level procedures handling the registry
 *
 *
 * Copyright (c) 1996-1999 Andreas Kupries (a.kupries@westend.com)
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL I LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS
 * SOFTWARE AND ITS DOCUMENTATION, EVEN IF I HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * I SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND
 * I HAVE NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
 * ENHANCEMENTS, OR MODIFICATIONS.
 *
 * CVS: $Id: registry.c,v 1.56 2007/10/12 22:49:55 andreas_kupries Exp $
 */

#include "transformInt.h"

/*
 * Code used to associate the registry with an interpreter.
 */

#define ASSOC "binTrf"

#ifdef TRF_DEBUG
int n = 0;
#endif

/*
 * Possible values for 'flags' field in control structure.
 */
#define CHANNEL_ASYNC		(1<<0) /* non-blocking mode */

/*
 * Number of milliseconds to wait before firing an event to flush
 * out information waiting in buffers (fileevent support).
 *
 * Relevant for only Tcl 8.0 and beyond.
 */

#define TRF_DELAY (5)

/*
 * Structures used by an attached transformation procedure
 *
 * => Information stored for a single direction of the channel.
 * => Information required by a result buffer.
 * => Information stored for the complete channel.
 */

typedef struct _DirectionInfo_ {
  Trf_ControlBlock   control; /* control block of transformation */
  Trf_Vectors*       vectors; /* vectors used during the transformation */
} DirectionInfo;


/*
 * Definition of the structure containing the information about the
 * internal input buffer.
 */

typedef struct _SeekState_ SeekState;

typedef struct _ResultBuffer_ {
  unsigned char* buf;       /* Reference to the buffer area */
  int            allocated; /* Allocated size of the buffer area */
  int            used;      /* Number of bytes in the buffer, <= allocated */

  SeekState*    seekState;
} ResultBuffer;


typedef struct _SeekConfig_ {

  int          overideAllowed; /* Boolean flag. If set the user may overide the
				* standard policy with his own choice */
  Trf_SeekInformation natural; /* Natural seek policy, copied from the
				* transform definition */
  Trf_SeekInformation  chosen;  /* Seek policy chosen from natural policy
				 * and the underlying channels; */
  int identity;                 /* Flag, set if 'identity' was forced by the
				 * user. */
} SeekConfig;


struct _SeekState_ {
  /* -- Integrity conditions --
   *
   * BufStartLoc == BufEndLoc	implies 	ResultLength(&result) == 0.
   * BufStartLoc == BufEndLoc	implies		UpLoc == BufStart.
   *
   * UP_CONVERT (DownLoc - AheadOffset) == BufEndLoc
   *
   * UpXLoc % seekState.used.numBytesTransform == 0
   * <=> Transform may seek only in multiples of its input tuples.
   *
   * (DownLoc - AheadOffset) % seekState.used.numBytesDown == 0
   * <=> Downstream channel operates in multiples of the transformation
   *     output tuples, except for possible offsets because of read ahead.
   *
   * UP_CONVERT (DownZero) == 0
   *
   * -- Integrity conditions --
   */

  Trf_SeekInformation    used;  /* Seek policy currently in effect, might
				 * be chosen by user */
  int                 allowed;  /* Flag. Set for seekable transforms. Derived
				 * from the contents of 'used'. */

  int upLoc;         /* Current location of file pointer in the
		      * transformed stream. */
  int upBufStartLoc; /* Same as above, for start of read buffer (result) */
  int upBufEndLoc;   /* See above, for the character after the end of the
		      * buffer. */
  int downLoc;       /* Current location of the file pointer in the channel
		      * downstream. */
  int downZero;      /* location downstream equivalent to UpLoc == 0 */
  int aheadOffset;   /* #Bytes DownLoc is after the down location of
		      * BufEnd. Values > 0 indicate incomplete data in the
		      * transform buffer itself. */
  int changed;       /* Flag, set if seeking occured with 'identity' set */
};


/** XXX change definition for 8.2, at compile time */

typedef struct _TrfTransformationInstance_ {
#ifdef USE_TCL_STUBS
  int patchVariant; /* See transformInt.h, Trf_Registry */
#endif

  /* 04/13/1999 Fileevent patch from Matt Newman <matt@novadigm.com> */

  Tcl_Channel self;   /* Our own channel handle */
  Tcl_Channel parent; /* The channel we are stacked upon. Relevant
		       * only for values PATCH_ORIG and PATCH_832 of
		       * 'patchVariant', see above. */

  int readIsFlushed; /* flag to note wether in.flushProc was called or not */

  /* 04/13/1999 Fileevent patch from Matt Newman <matt@novadigm.com> */

  int flags;         /* currently CHANNEL_ASYNC or zero */
  int watchMask;     /* current TrfWatch mask */
  
  int mode;          /* mode of parent channel,
		      * OR'ed combination of
		      * TCL_READABLE, TCL_WRITABLE */

  /* Tcl_Transformation standard; data required for all transformation
   * instances.
   */
  DirectionInfo      in;   /* information for transformation of read data */
  DirectionInfo      out;  /* information for transformation of written data */
  ClientData         clientData; /* copy from entry->trfType->clientData */

  /*
   * internal result buffer used during transformations of incoming data.
   * Stores results waiting for retrieval too, i.e. state information
   * carried from call to call.
   */

  ResultBuffer result;

  /* Number of bytes written during a down transformation.
   */

  int lastWritten;

  /* Number of bytes stored during an up transformation
   */

  int lastStored;


  /* Timer for automatic push out of information sitting in various channel
   * buffers. Used by the fileevent support. See 'ChannelHandler'.
   */

  Tcl_TimerToken timer;

  /* Information about the chosen and used seek policy and wether the user
   * is allowed to change it. Runtime configuration.
   */

  SeekConfig seekCfg;

  /* More seek information, runtime state.
   */

  SeekState seekState;

#ifdef TRF_STREAM_DEBUG
  char*         name;       /* Name of transformation command */
  unsigned long inCounter;  /* Number of bytes read from below */
  unsigned long outCounter; /* Number of bytes stored in 'result' */
#endif

} TrfTransformationInstance;

#ifdef TRF_STREAM_DEBUG
#define STREAM_IN(trans,blen,buf) {int i; for (i=0;i<(blen);i++,(trans)->inCounter++) {printf ("%p:%s:in_\t%d\t%02x\n", (trans), (trans)->name, (trans)->inCounter, 0xff & ((buf) [i]));}}
#define STREAM_OUT(trans,blen,buf) {int i; for (i=0;i<(blen);i++,(trans)->outCounter++) {printf ("%p:%s:out\t%d\t%02x\n", (trans), (trans)->name, (trans)->outCounter, 0xff & ((buf) [i]));}}
#else
#define STREAM_IN(t,bl,b)
#define STREAM_OUT(t,bl,b)
#endif


#define INCREMENT (512)
#define READ_CHUNK_SIZE 4096


#define TRF_UP_CONVERT(trans,k) \
     (((k) / trans->seekState.used.numBytesDown) * trans->seekState.used.numBytesTransform)

#define TRF_DOWN_CONVERT(trans,k) \
     (((k) / trans->seekState.used.numBytesTransform) * trans->seekState.used.numBytesDown)

#define TRF_IS_UNSEEKABLE(si) \
     (((si).numBytesTransform == 0) || ((si).numBytesDown == 0))

#define TRF_SET_UNSEEKABLE(si) \
     {(si).numBytesTransform = 0 ; (si).numBytesDown = 0;}



/*
 * forward declarations of all internally used procedures.
 */

static Tcl_ChannelType*
AllocChannelType _ANSI_ARGS_ ((int* sizePtr));

static Tcl_ChannelType*
InitializeChannelType _ANSI_ARGS_ ((CONST char* name, int patchVariant));


static int
TrfUnregister _ANSI_ARGS_ ((Tcl_Interp*       interp,
                            Trf_RegistryEntry* entry));

static void
TrfDeleteRegistry _ANSI_ARGS_ ((ClientData clientData, Tcl_Interp *interp));

static int
TrfExecuteObjCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp* interp,
			      int objc, struct Tcl_Obj* CONST objv []));

static void
TrfDeleteCmd _ANSI_ARGS_((ClientData clientData));

#if 0
static int
TrfInfoObjCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp* interp,
			   int objc, struct Tcl_Obj* CONST objv []));
#endif
/* 04/13/1999 Fileevent patch from Matt Newman <matt@novadigm.com>
 */
static int
TrfBlock _ANSI_ARGS_ ((ClientData instanceData, int mode));

static int
TrfClose _ANSI_ARGS_ ((ClientData instanceData, Tcl_Interp* interp));

static int
TrfInput _ANSI_ARGS_ ((ClientData instanceData,
		       char* buf, int toRead,
		       int*       errorCodePtr));

static int
TrfOutput _ANSI_ARGS_ ((ClientData instanceData,
			CONST84 char* buf, int toWrite,
			int*        errorCodePtr));

static int
TrfSeek _ANSI_ARGS_ ((ClientData instanceData, long offset,
		      int mode, int* errorCodePtr));
static void
TrfWatch _ANSI_ARGS_ ((ClientData instanceData, int mask));

static int
TrfGetFile _ANSI_ARGS_ ((ClientData instanceData, int direction,
			 ClientData* handlePtr));

static int
TrfGetOption _ANSI_ARGS_ ((ClientData instanceData, Tcl_Interp* interp,
			   CONST84 char* optionName, Tcl_DString* dsPtr));

static int
TrfSetOption _ANSI_ARGS_((ClientData instanceData, Tcl_Interp* interp,
			  CONST char* optionName, CONST char* value));
#ifdef USE_TCL_STUBS
static int
TrfNotify _ANSI_ARGS_((ClientData instanceData, int interestMask));
#endif

static int
TransformImmediate _ANSI_ARGS_ ((Tcl_Interp* interp, Trf_RegistryEntry* entry,
				 Tcl_Channel source, Tcl_Channel destination,
				 struct Tcl_Obj* CONST in,
				 Trf_Options optInfo));

static int
AttachTransform _ANSI_ARGS_ ((Trf_RegistryEntry* entry,
			      Trf_BaseOptions*   baseOpt,
			      Trf_Options        optInfo,
			      Tcl_Interp*        interp));

static int
PutDestination _ANSI_ARGS_ ((ClientData clientData,
                             unsigned char* outString, int outLen,
                             Tcl_Interp* interp));

static int
PutDestinationImm _ANSI_ARGS_ ((ClientData clientData,
				unsigned char* outString, int outLen,
				Tcl_Interp* interp));
static int
PutTrans _ANSI_ARGS_ ((ClientData clientData,
		       unsigned char* outString, int outLen,
		       Tcl_Interp* interp));

static int
PutInterpResult _ANSI_ARGS_ ((ClientData clientData,
			      unsigned char* outString, int outLen,
			      Tcl_Interp* interp));
/* 04/13/1999 Fileevent patch from Matt Newman <matt@novadigm.com>
 */
static void
ChannelHandler _ANSI_ARGS_ ((ClientData clientData, int mask));

static void
ChannelHandlerTimer _ANSI_ARGS_ ((ClientData clientData));

#ifdef USE_TCL_STUBS
static Tcl_Channel
DownChannel _ANSI_ARGS_ ((TrfTransformationInstance* ctrl));

static int
DownSeek _ANSI_ARGS_ ((TrfTransformationInstance* ctrl, int offset, int mode));

static int
DownRead _ANSI_ARGS_ ((TrfTransformationInstance* ctrl,
		       char* buf, int toRead));
static int
DownWrite _ANSI_ARGS_ ((TrfTransformationInstance* ctrl,
		       char* buf, int toWrite));
static int
DownSOpt _ANSI_ARGS_ ((Tcl_Interp* interp,
		       TrfTransformationInstance* ctrl,
		       CONST char* optionName, CONST char* value));
static int
DownGOpt _ANSI_ARGS_ ((Tcl_Interp* interp,
		       TrfTransformationInstance* ctrl,
		       CONST84 char* optionName, Tcl_DString* dsPtr));

#define DOWNC(trans)             (DownChannel (trans))
#define TELL(trans)              (SEEK (trans, 0, SEEK_CUR))
#define SEEK(trans,off,mode)     (DownSeek  ((trans), (off), (mode)))
#define READ(trans,buf,toRead)   (DownRead  ((trans), (buf), (toRead)))
#define WRITE(trans,buf,toWrite) (DownWrite ((trans), (buf), (toWrite)))
#define SETOPT(i,trans,opt,val)  (DownSOpt  ((i), (trans), (opt), (val)))
#define GETOPT(i,trans,opt,ds)   (DownGOpt  ((i), (trans), (opt), (ds)))
#else
#define DOWNC(trans)             ((trans)->parent)
#define TELL(trans)              (SEEK (trans, 0, SEEK_CUR))
#define SEEK(trans,off,mode)     (Tcl_Seek  ((trans)->parent, (off), (mode)))
#define READ(trans,buf,toRead)   (Tcl_Read  ((trans)->parent, (buf), (toRead)))
#define WRITE(trans,buf,toWrite) (Tcl_Write ((trans)->parent, (buf), (toWrite)))
#define SETOPT(i,trans,opt,val)  (Tcl_SetChannelOption ((i), (trans)->parent, (opt), (val)))
#define GETOPT(i,trans,opt,ds)   (Tcl_GetChannelOption ((i), (trans)->parent, (opt), (ds)))
#endif

/* Convenience macro for allocation
 * of new transformation instances.
 */

#define NEW_TRANSFORM \
(TrfTransformationInstance*) Tcl_Alloc (sizeof (TrfTransformationInstance));

/* Procedures to handle the internal timer.
 */

static void
TimerKill _ANSI_ARGS_ ((TrfTransformationInstance* trans));

static void
TimerSetup _ANSI_ARGS_ ((TrfTransformationInstance* trans));

static void
ChannelHandlerKS _ANSI_ARGS_ ((TrfTransformationInstance* trans, int mask));



/* Procedures to handle the internal read buffer.
 */

static void             ResultClear  _ANSI_ARGS_ ((ResultBuffer* r));
static void             ResultInit   _ANSI_ARGS_ ((ResultBuffer* r));
static int              ResultLength _ANSI_ARGS_ ((ResultBuffer* r));
static int              ResultCopy   _ANSI_ARGS_ ((ResultBuffer* r,
			    unsigned char* buf, int toRead));
static void             ResultDiscardAtStart _ANSI_ARGS_ ((ResultBuffer* r,
							   int n));
static void             ResultAdd    _ANSI_ARGS_ ((ResultBuffer* r,
                            unsigned char* buf, int toWrite));

/*
 * Procedures to handle seeking information.
 */

static void
SeekCalculatePolicies _ANSI_ARGS_ ((TrfTransformationInstance* trans));

static void
SeekInitialize _ANSI_ARGS_ ((TrfTransformationInstance* trans));

static void
SeekClearBuffer _ANSI_ARGS_ ((TrfTransformationInstance* trans, int which));

static void
SeekSynchronize _ANSI_ARGS_ ((TrfTransformationInstance* trans,
			      Tcl_Channel parent));

static Tcl_Obj*
SeekStateGet _ANSI_ARGS_ ((Tcl_Interp* interp, SeekState* state));

static Tcl_Obj*
SeekConfigGet _ANSI_ARGS_ ((Tcl_Interp* interp, SeekConfig* cfg));

static void
SeekPolicyGet _ANSI_ARGS_ ((TrfTransformationInstance* trans,
			    char*                      policy));

#ifdef TRF_DEBUG
static void
SeekDump _ANSI_ARGS_ ((TrfTransformationInstance* trans, CONST char* place));

#define SEEK_DUMP(str) SeekDump (trans, #str)
#else
#define SEEK_DUMP(str)
#endif

/*
 *------------------------------------------------------*
 *
 *	TrfGetRegistry --
 *
 *	------------------------------------------------*
 *	Accessor to the interpreter associated registry
 *	of transformations.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Allocates and initializes the hashtable
 *		during the first call and associates it
 *		with the specified interpreter.
 *
 *	Result:
 *		The internal registry of transformations.
 *
 *------------------------------------------------------*
 */

Trf_Registry*
TrfGetRegistry (interp)
Tcl_Interp* interp;
{
  Trf_Registry* registry;

  START (TrfGetRegistry);

  registry = TrfPeekForRegistry (interp);

  if (registry == (Trf_Registry*) NULL) {
    registry           = (Trf_Registry*)  Tcl_Alloc (sizeof (Trf_Registry));
    registry->registry = (Tcl_HashTable*) Tcl_Alloc (sizeof (Tcl_HashTable));

    Tcl_InitHashTable (registry->registry, TCL_STRING_KEYS);

    Tcl_SetAssocData (interp, ASSOC, TrfDeleteRegistry,
		      (ClientData) registry);
  }

  DONE (TrfGetRegistry);
  return registry;
}

/*
 *------------------------------------------------------*
 *
 *	TrfPeekForRegistry --
 *
 *	------------------------------------------------*
 *	Accessor to the interpreter associated registry
 *	of transformations. Does not create the registry
 *	(in contrast to 'TrfGetRegistry').
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		The internal registry of transformations.
 *
 *------------------------------------------------------*
 */

Trf_Registry*
TrfPeekForRegistry (interp)
Tcl_Interp* interp;
{
  Tcl_InterpDeleteProc* proc;

  START (TrfPeekForRegistry);

  proc = TrfDeleteRegistry;

  DONE (TrfPeekForRegistry);
  return (Trf_Registry*) Tcl_GetAssocData (interp, ASSOC, &proc);
}

/*
 *------------------------------------------------------*
 *
 *	Trf_Register --
 *
 *	------------------------------------------------*
 *	Announce a transformation to the registry associated
 *	with the specified interpreter.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		May create the registry. Allocates and
 *		initializes the structure describing
 *		the announced transformation.
 *
 *	Result:
 *		A standard TCL error code.
 *
 *------------------------------------------------------*
 */

int
Trf_Register (interp, type)
Tcl_Interp*               interp;
CONST Trf_TypeDefinition* type;
{
  Trf_Registry*      registry;
  Trf_RegistryEntry* entry;
  Tcl_HashEntry*     hPtr;
  int                new;

  START (Trf_Register);
  PRINT ("(%p, \"%s\")\n", type, type->name); FL;

  registry = TrfGetRegistry (interp);

  /*
   * Already defined ?
   */

  hPtr = Tcl_FindHashEntry (registry->registry, (char*) type->name);

  if (hPtr != (Tcl_HashEntry*) NULL) {
    PRINT ("Already defined!\n"); FL;
    DONE (Trf_Register);
    return TCL_ERROR;
  }

  /*
   * Check validity of given structure
   */

#define IMPLY(a,b) ((! (a)) || (b))

  /* assert (type->options); */
  assert (IMPLY(type->options != NULL, type->options->createProc != NULL));
  assert (IMPLY(type->options != NULL, type->options->deleteProc != NULL));
  assert (IMPLY(type->options != NULL, type->options->checkProc  != NULL));
  assert (IMPLY(type->options != NULL,
		(type->options->setProc   != NULL) ||
		(type->options->setObjProc != NULL)));
  assert (IMPLY(type->options != NULL, type->options->queryProc  != NULL));

  assert (type->encoder.createProc);
  assert (type->encoder.deleteProc);
  assert ((type->encoder.convertProc != NULL) ||
	  (type->encoder.convertBufProc != NULL));
  assert (type->encoder.flushProc);
  assert (type->encoder.clearProc);

  assert (type->decoder.createProc);
  assert (type->decoder.deleteProc);
  assert ((type->decoder.convertProc != NULL) ||
	  (type->decoder.convertBufProc != NULL));
  assert (type->decoder.flushProc);
  assert (type->decoder.clearProc);

  /*
   * Generate command to execute transformations immediately or to generate
   * filters.
   */

  entry          = (Trf_RegistryEntry*) Tcl_Alloc (sizeof (Trf_RegistryEntry));
  entry->registry   = registry;

  entry->trfType    = (Trf_TypeDefinition*) type;
  entry->interp     = interp;
#ifndef USE_TCL_STUBS
  entry->transType  = InitializeChannelType (type->name, -1);
#else
  entry->transType  = InitializeChannelType (type->name,
					     registry->patchVariant);
#endif
  entry->trfCommand = Tcl_CreateObjCommand (interp, (char*) type->name,
					    TrfExecuteObjCmd,
					    (ClientData) entry, TrfDeleteCmd);

  /*
   * Add entry to internal registry.
   */

  hPtr = Tcl_CreateHashEntry (registry->registry, (char*) type->name, &new);
  Tcl_SetHashValue (hPtr, entry);

  DONE (Trf_Register);
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	Trf_Unregister --
 *
 *	------------------------------------------------*
 *	Removes the transformation from the registry
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Releases the memory allocated in 'Trf_Register'.
 *
 *	Result:
 *		A standard TCL error code.
 *
 *------------------------------------------------------*
 */

static int
TrfUnregister (interp, entry)
Tcl_Interp*        interp;
Trf_RegistryEntry* entry;
{
  Trf_Registry*  registry;
  Tcl_HashEntry* hPtr;

  START (Trf_Unregister);

  registry  = TrfGetRegistry    (interp);
  hPtr      = Tcl_FindHashEntry (registry->registry,
				 (char*) entry->trfType->name);

  Tcl_Free ((char*) entry->transType);
  Tcl_Free ((char*) entry);

  Tcl_DeleteHashEntry (hPtr);

  DONE (Trf_Unregister);
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	TrfDeleteRegistry --
 *
 *	------------------------------------------------*
 *	Trap handler. Called by the Tcl core during
 *	interpreter destruction. Destroys the registry
 *	of transformations.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Releases the memory allocated in 'TrfGetRegistry'.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
TrfDeleteRegistry (clientData, interp)
ClientData  clientData;
Tcl_Interp* interp;
{
  Trf_Registry* registry = (Trf_Registry*) clientData;

  START (TrfDeleteRegistry);

  /*
   * The commands are already deleted, therefore the hashtable is empty here.
   */

  Tcl_DeleteHashTable (registry->registry);
  Tcl_Free ((char*) registry);

  DONE (TrfDeleteRegistry);
}

/* (readable) shortcuts for calling the option processing vectors.
 */

#define CLT  (entry->trfType->clientData)
#define OPT  (entry->trfType->options)

#define CREATE_OPTINFO         (OPT ? (*OPT->createProc) (CLT) : NULL)
#define DELETE_OPTINFO         if (optInfo) (*OPT->deleteProc) (optInfo, CLT)
#define CHECK_OPTINFO(baseOpt) (optInfo ? (*OPT->checkProc) (optInfo, interp, &baseOpt, CLT) : TCL_OK)
#define SET_OPTION(opt,optval) (optInfo ? (*OPT->setProc) (optInfo, interp, opt, optval, CLT) : TCL_ERROR)

#define SET_OPTION_OBJ(opt,optval) (optInfo ? (*OPT->setObjProc) (optInfo, interp, opt, optval, CLT) : TCL_ERROR)

#define ENCODE_REQUEST(entry,optInfo) (optInfo ? (*OPT->queryProc) (optInfo, CLT) : 1)

/*
 *------------------------------------------------------*
 *
 *	TrfExecuteObjCmd --
 *
 *	------------------------------------------------*
 *	Implementation procedure for all transformations.
 *	Equivalent to 'TrfExecuteCmd', but using the new
 *	Object interfaces.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		See 'TrfExecuteCmd'.
 *
 *	Result:
 *		A standard TCL error code.
 *
 *------------------------------------------------------*
 */

static int
TrfExecuteObjCmd (clientData, interp, objc, objv)
     ClientData              clientData;
     Tcl_Interp*             interp;
     int                     objc;
     struct Tcl_Obj* CONST * objv;
{
  /* (readable) shortcuts for calling the option processing vectors.
   * as defined in 'TrfExecuteCmd'.
   */

  int                res, len;
  /*  Tcl_Channel        source, destination;*/
  /*  int                src_mode, dst_mode;*/
  const char*        cmd;
  const char*        option;
  struct Tcl_Obj*    optarg;
  Trf_RegistryEntry* entry;
  Trf_Options        optInfo;
  Trf_BaseOptions    baseOpt;
  int                mode;
  int                wrong_mod2;
  int                wrong_number;

  START (TrfExecuteObjCmd);
#ifdef TRF_DEBUG
  {
    int i;
    for (i = 0; i < objc; i++) {
      PRINT ("Argument [%03d] = \"%s\"\n",
	     i, Tcl_GetStringFromObj (objv [i], NULL)); FL;
    }
  }
#endif

  baseOpt.attach      = (Tcl_Channel) NULL;
  baseOpt.attach_mode = 0;
  baseOpt.source      = (Tcl_Channel) NULL;
  baseOpt.destination = (Tcl_Channel) NULL;
  baseOpt.policy      = (Tcl_Obj*)    NULL;

  entry = (Trf_RegistryEntry*) clientData;
  cmd   = Tcl_GetStringFromObj (objv [0], NULL);

  objc --;
  objv ++;

  optInfo = CREATE_OPTINFO;

  PRINT ("Processing options...\n"); FL; IN;

  while ((objc > 0) && (*Tcl_GetStringFromObj (objv [0], NULL) == '-')) {
    /*
     * Process options, as long as they are found
     */

    option = Tcl_GetStringFromObj (objv [0], NULL);

    if (0 == strcmp (option, "--")) {
      /* end of option list */
      objc--, objv++;
      break;
    }

    wrong_number = (objc < 2); /* option, but without argument */

    optarg = objv [1];

    objc -= 2;
    objv += 2;

    len = strlen (option);
    
    if (len < 2)
      goto unknown_option;

    switch (option [1])
      {
      case 'a':
	if (0 != strncmp (option, "-attach", len))
	  goto check_for_trans_option;

	if (wrong_number) {
	  Tcl_AppendResult (interp, cmd, ": wrong # args, option \"", option, "\" requires an argument", (char*) NULL);
	  OT;
	  goto cleanup_after_error;      
	}

	baseOpt.attach = Tcl_GetChannel (interp,
					 Tcl_GetStringFromObj (optarg, NULL),
					 &baseOpt.attach_mode);
	if (baseOpt.attach == (Tcl_Channel) NULL) {
	  OT;
	  goto cleanup_after_error;
	}
	break;

      case 'i':
	if (0 != strncmp (option, "-in", len))
	  goto check_for_trans_option;

	if (wrong_number) {
	  Tcl_AppendResult (interp, cmd, ": wrong # args, option \"", option, "\" requires an argument", (char*) NULL);
	  OT;
	  goto cleanup_after_error;      
	}

	baseOpt.source = Tcl_GetChannel (interp,
					 Tcl_GetStringFromObj (optarg, NULL),
					 &mode);
	if (baseOpt.source == (Tcl_Channel) NULL)
	  goto cleanup_after_error;

	if (! (mode & TCL_READABLE)) {
	  Tcl_AppendResult (interp, cmd,
			    ": source-channel not readable",
			    (char*) NULL);
	  OT;
	  goto cleanup_after_error;
	}
	break;

      case 'o':
	if (0 != strncmp (option, "-out", len))
	  goto check_for_trans_option;

	if (wrong_number) {
	  Tcl_AppendResult (interp, cmd, ": wrong # args, option \"", option, "\" requires an argument", (char*) NULL);
	  OT;
	  goto cleanup_after_error;      
	}

	baseOpt.destination = Tcl_GetChannel (interp,
					      Tcl_GetStringFromObj (optarg,
								    NULL),
					      &mode);

	if (baseOpt.destination == (Tcl_Channel) NULL) {
	  OT;
	  goto cleanup_after_error;
	}

	if (! (mode & TCL_WRITABLE)) {
	  Tcl_AppendResult (interp, cmd,
			    ": destination-channel not writable",
			    (char*) NULL);
	  OT;
	  goto cleanup_after_error;
	}
	break;

      case 's':
	if (0 != strncmp (option, "-seekpolicy", len))
	  goto check_for_trans_option;

	if (wrong_number) {
	  Tcl_AppendResult (interp, cmd, ": wrong # args, option \"", option, "\" requires an argument", (char*) NULL);
	  OT;
	  goto cleanup_after_error;      
	}

	baseOpt.policy = optarg;
	Tcl_IncrRefCount (optarg);
	break;

      default:
      check_for_trans_option:
	if (wrong_number) {
	  Tcl_AppendResult (interp, cmd, ": wrong # args, all options require an argument", (char*) NULL);
	  OT;
	  goto cleanup_after_error;      
	}

	if ((*OPT->setObjProc) == NULL) {
	  res = SET_OPTION     (option, Tcl_GetStringFromObj (optarg, NULL));
	} else {
	  res = SET_OPTION_OBJ (option, optarg);
	}

	if (res != TCL_OK) {
	  OT;
	  goto cleanup_after_error;
	}
	break;
      } /* switch option */
  } /* while options */

  OT;

  /*
   * Check argument restrictions, insert defaults if necessary,
   * execute the required operation.
   */

  if ((baseOpt.attach != (Tcl_Channel) NULL) &&
      ((baseOpt.source      != (Tcl_Channel) NULL) ||
       (baseOpt.destination != (Tcl_Channel) NULL))) {
    Tcl_AppendResult (interp, cmd,
	      ": inconsistent options, -in/-out not allowed with -attach",
		      (char*) NULL);

    PRINT ("Inconsistent options\n"); FL;
    goto cleanup_after_error;
  }

  if ((baseOpt.attach == (Tcl_Channel) NULL) &&
      baseOpt.policy !=  (Tcl_Obj*) NULL) {

    Tcl_AppendResult (interp, cmd,
		      ": inconsistent options, -seekpolicy ",
		      "not allowed without -attach",
		      (char*) NULL);

    PRINT ("Inconsistent options\n"); FL;
    goto cleanup_after_error;
  }

  if ((baseOpt.source == (Tcl_Channel) NULL) &&
      (baseOpt.attach == (Tcl_Channel) NULL))
    wrong_mod2 = 0;
  else
    wrong_mod2 = 1;

  if (wrong_mod2 == (objc % 2)) {
      Tcl_AppendResult (interp, cmd, ": wrong # args", (char*) NULL);
      PRINT ("Wrong # args\n"); FL;
      goto cleanup_after_error;
  }

  res = CHECK_OPTINFO (baseOpt);
  if (res != TCL_OK) {
    DELETE_OPTINFO;

    PRINT ("Options contain errors\n"); FL;
    DONE (TrfExecuteObjCmd);
    return TCL_ERROR;
  }

  if (baseOpt.attach == (Tcl_Channel) NULL) /* TRF_IMMEDIATE */ {
    /*
     * Immediate execution of transformation requested.
     */

    res = TransformImmediate (interp, entry,
			      baseOpt.source, baseOpt.destination,
			      objv [0], optInfo);

  } else /* TRF_ATTACH */ {
    /*
     * User requested attachment of transformation procedure to a channel.
     * In case of a stub-aware interpreter use that to check for the
     * existence of the necessary patches ! Bail out if not.
     */

#ifdef USE_TCL_STUBS
    if (Tcl_StackChannel == NULL) {
      Tcl_AppendResult (interp, cmd, ": this feature (-attach) is not ",
			"available as the required patch to the core ",
			"was not applied", (char*) NULL);
      DELETE_OPTINFO;

      PRINT ("-attach not available\n"); FL;
      DONE (TrfExecuteObjCmd);
      return TCL_ERROR;
    }
#endif

    res = AttachTransform (entry, &baseOpt, optInfo, interp);

    if (baseOpt.policy != (Tcl_Obj*) NULL) {
      Tcl_DecrRefCount (baseOpt.policy);
      baseOpt.policy = (Tcl_Obj*) NULL;
    }
  }

  DELETE_OPTINFO;
  DONE (TrfExecuteObjCmd);
  return res;


unknown_option:
  PRINT ("Unknown option \"%s\"\n", option); FL; OT;

  Tcl_AppendResult (interp, cmd, ": unknown option '", option, "', should be '-attach/in/out' or '-seekpolicy'",
		    (char*) NULL);
  /* fall through to cleanup */

cleanup_after_error:
  DELETE_OPTINFO;
  DONE (TrfExecuteObjCmd);
  return TCL_ERROR;
}

/*
 *------------------------------------------------------*
 *
 *	TrfDeleteCmd --
 *
 *	------------------------------------------------*
 *	Trap handler. Called by the Tcl core during
 *	destruction of the command for invocation of a
 *	transformation.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Removes the transformation from the registry.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
TrfDeleteCmd (clientData)
ClientData clientData;
{
  Trf_RegistryEntry* entry;

  START (TrfDeleteCmd);

  entry = (Trf_RegistryEntry*) clientData;

  TrfUnregister (entry->interp, entry);
  DONE (TrfDeleteCmd);
}

/*
 *----------------------------------------------------------------------
 *
 * TrfInfoObjCmd --
 *
 *	This procedure is invoked to process the "trfinfo" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
#if 0
static int
TrfInfoObjCmd (notUsed, interp, objc, objv)
     ClientData              notUsed;	/* Not used. */
     Tcl_Interp*             interp;	/* Current interpreter. */
     int                     objc;
     struct Tcl_Obj* CONST * objv;
{
  /*
   * trfinfo <channel>
   */

  static char* subcmd [] = {
    "seekstate", "seekcfg", NULL
  };
  enum subcmd {
    TRFINFO_SEEKSTATE, TRFINFO_SEEKCFG
  };	  

  Tcl_Channel                chan;
  int                        mode, pindex;
  char*                      chanName;
  TrfTransformationInstance* trans;


  if ((objc < 2) || (objc > 3)) {
    Tcl_AppendResult (interp,
		      "wrong # args: should be \"trfinfo cmd channel\"",
		      (char*) NULL);
    return TCL_ERROR;
  }

  chanName = Tcl_GetStringFromObj (objv [2], NULL);
  chan     = Tcl_GetChannel (interp, chanName, &mode);

  if (chan == (Tcl_Channel) NULL) {
    return TCL_ERROR;
  }

  if (Tcl_GetChannelType (chan)->seekProc != TrfSeek) {
    /* No trf transformation, info not applicable.
     */

    Tcl_AppendResult (interp,
		      "channel \"", chanName,
		      "\" is no transformation from trf",
		      (char*) NULL);
    return TCL_ERROR;
  }

  /* Peek into the instance structure and return the requested
   * information.
   */

  if (Tcl_GetIndexFromObj(interp, objv [1], subcmd, "subcommand", 0,
			  &pindex) != TCL_OK) {
    return TCL_ERROR;
  }

  trans = (TrfTransformationInstance*) Tcl_GetChannelInstanceData (chan);

  switch (pindex) {
  case TRFINFO_SEEKSTATE:
    {
      Tcl_Obj* state = SeekStateGet (interp, &trans->seekState);

      if (state == NULL)
	return TCL_ERROR;

      Tcl_SetObjResult (interp, state);
      return TCL_OK;
    }
    break;

  case TRFINFO_SEEKCFG:
    {
      Tcl_Obj* cfg = SeekConfigGet (interp, &trans->seekCfg);

      if (cfg == NULL)
	return TCL_ERROR;

      Tcl_SetObjResult (interp, cfg);
      return TCL_OK;
    }
    break;

  default:
    /* impossible */
    return TCL_ERROR;
  }

  /* We should not come to this place */
  return TCL_ERROR;
}
#endif

/*
 *------------------------------------------------------*
 *
 *	TrfInit_Info --
 *
 *	------------------------------------------------*
 *	Register the 'info' command.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of 'Tcl_CreateObjCommand'.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

int
TrfInit_Info (interp)
Tcl_Interp* interp;
{
#if 0
  Tcl_CreateObjCommand (interp, "trfinfo", TrfInfoObjCmd,
			(ClientData) NULL,
			(Tcl_CmdDeleteProc *) NULL);
#endif
  return TCL_OK;
}

/* 04/13/1999 Fileevent patch from Matt Newman <matt@novadigm.com>
 */
/*
 *------------------------------------------------------*
 *
 *	TrfBlock --
 *
 *	------------------------------------------------*
 *	Trap handler. Called by the generic IO system
 *	during option processing to change the blocking
 *	mode of the channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Forwards the request to the underlying
 *		channel.
 *
 *	Result:
 *		0 if successful, errno when failed.
 *
 *------------------------------------------------------*
 */

static int
TrfBlock (instanceData, mode)
ClientData  instanceData;
int mode;
{
  TrfTransformationInstance* trans = (TrfTransformationInstance*) instanceData;
  char                   block [2] = {0,0};
  Tcl_Channel            parent;

  START (TrfBlock);
  PRINT ("Mode = %d\n", mode); FL;

  parent = DOWNC (trans);

  if (mode == TCL_MODE_NONBLOCKING) {
    trans->flags |= CHANNEL_ASYNC;
    block [0] = '0';
  } else {
    trans->flags &= ~(CHANNEL_ASYNC);
    block [0] = '1';
  }

#ifndef USE_TCL_STUBS
  Tcl_SetChannelOption (NULL, parent, "-blocking", block);
#else
  if ((trans->patchVariant == PATCH_ORIG) ||
      (trans->patchVariant == PATCH_82)) {
    /*
     * Both old-style patch and first integrated version of the patch
     * require the transformation to pass the blocking mode to the
     * channel downstream. The newest implementation (PATCH_832)
     * handles this in the core.
     */

    Tcl_SetChannelOption (NULL, parent, "-blocking", block);
  }
#endif

  DONE (TrfBlock);
  return 0;
}

/*
 *------------------------------------------------------*
 *
 *	TrfClose --
 *
 *	------------------------------------------------*
 *	Trap handler. Called by the generic IO system
 *	during destruction of the transformation channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Releases the memory allocated in
 *		'AttachTransform'.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static int
TrfClose (instanceData, interp)
ClientData  instanceData;
Tcl_Interp* interp;
{
  /*
   * The parent channel will be removed automatically
   * (if necessary and/or desired).
   */

  TrfTransformationInstance* trans = (TrfTransformationInstance*) instanceData;
  Tcl_Channel               parent;

  START (TrfClose);

#ifndef USE_TCL_STUBS
  if ((trans  == (TrfTransformationInstance*) NULL) ||
      (interp == (Tcl_Interp*) NULL)) {
    /* Hack, prevent 8.0 from crashing upon exit if channels
     * with transformations were left open during exit
     *
     * Suggested by Mikhail Teterin <mi@aldan.algebra.com> 25.11.1999.
     */

    DONE (TrfClose);
    return TCL_OK;
  }
#endif

  parent = DOWNC (trans);

  /* 04/13/1999 Fileevent patch from Matt Newman <matt@novadigm.com>
   * Remove event handler to underlying channel, this could
   * be because we are closing for real, or being "unstacked".
   */

#ifndef USE_TCL_STUBS
  Tcl_DeleteChannelHandler (parent, ChannelHandler, (ClientData) trans);
#else
  if ((trans->patchVariant == PATCH_ORIG) ||
      (trans->patchVariant == PATCH_82)) {
    Tcl_DeleteChannelHandler (parent, ChannelHandler, (ClientData) trans);
  }
  /*
   * PATCH_832 doesn't use channelhandlers for communication of events
   * between the channels of stack anymore.
   */
#endif

  TimerKill (trans);

  /*
   * Flush data waiting in transformation buffers to output.
   * Flush input too, maybe there are side effects other
   * parts do rely on (-> message digests).
   */

  if (trans->mode & TCL_WRITABLE) {
    PRINT ("out.flushproc\n"); FL;

    trans->out.vectors->flushProc (trans->out.control,
				   (Tcl_Interp*) NULL,
				   trans->clientData);
  }

  if (trans->mode & TCL_READABLE) {
    if (!trans->readIsFlushed) {
      PRINT ("in_.flushproc\n"); FL;

      trans->readIsFlushed = 1;
      trans->in.vectors->flushProc (trans->in.control,
				    (Tcl_Interp*) NULL,
				    trans->clientData);
    }
  }

  if (trans->mode & TCL_WRITABLE) {
    PRINT ("out.deleteproc\n"); FL;
    trans->out.vectors->deleteProc (trans->out.control, trans->clientData);
  }

  if (trans->mode & TCL_READABLE) {
    PRINT ("in_.deleteproc\n"); FL;
    trans->in.vectors->deleteProc  (trans->in.control,  trans->clientData);
  }

  ResultClear (&trans->result);

  DONE (TrfClose);
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	TrfInput --
 *
 *	------------------------------------------------*
 *	Called by the generic IO system to convert read data.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As defined by the conversion.
 *
 *	Result:
 *		A transformed buffer.
 *
 *------------------------------------------------------*
 */

static int
TrfInput (instanceData, buf, toRead, errorCodePtr)
ClientData instanceData;
char*      buf;
int        toRead;
int*       errorCodePtr;
{
  TrfTransformationInstance* trans = (TrfTransformationInstance*) instanceData;
  int       gotBytes, read, i, res, copied, maxRead;
  Tcl_Channel parent;

  START (TrfInput);
  PRINT ("trans = %p, toRead = %d\n", trans, toRead); FL;

  parent = DOWNC (trans);

  /* should assert (trans->mode & TCL_READABLE) */

  gotBytes = 0;

  SEEK_DUMP (TrfInput; Start);

  while (toRead > 0) {
    /* Loop until the request is satisfied
     * (or no data available from below, possibly EOF).
     */

    SEEK_DUMP (TrfInput; Loop_);

    /* The position may be inside the buffer, and not at its start.
     * Remove the superfluous data now. There was no need to do it
     * earlier, as intervening seeks and writes could have discarded
     * the buffer completely, seeked back to an earlier point in it, etc.
     * We can be sure that the location is not behind its end!
     * And for an empty buffer location and buffer start are identical,
     * bypassing this code. See integrity constraints listed in the
     * description of Trf_TransformationInstance.
     */

    if (trans->seekState.upLoc > trans->seekState.upBufStartLoc) {
      ResultDiscardAtStart (&trans->result,
		    trans->seekState.upLoc - trans->seekState.upBufStartLoc);
    }

    /* Assertion: UpLoc == UpBufStartLoc now. */

    SEEK_DUMP (TrfInput; Disc<);

    copied    = ResultCopy (&trans->result, (unsigned char*) buf, toRead);
    toRead   -= copied;
    buf      += copied;
    gotBytes += copied;
    trans->seekState.upLoc += copied;

    SEEK_DUMP (TrfInput; Copy<);

    if (toRead == 0) {
      PRINT ("Got %d, satisfied from result buffer\n", gotBytes); FL;
      DONE  (TrfInput);
      return gotBytes;
    }

    /* The buffer is exhausted, but the caller wants even more. We now have
     * to go to the underlying channel, get more bytes and then transform
     * them for delivery. We may not get that we want (full EOF or temporary
     * out of data). This part has to manipulate the various seek locations
     * in a more complicated way to keep everything in sync.
     */

    /* Assertion:    UpLoc == UpBufEndLoc now (and == UpBufStartLoc).
     * Additionally: UP_CONVERT (DownLoc - AheadOffset) == BufEndLoc
     */

    /*
     * Length (trans->result) == 0, toRead > 0 here  Use 'buf'! as target
     * to store the intermediary information read from the parent channel.
     *
     * Ask the transform how much data it allows us to read from
     * the underlying channel. This feature allows the transform to
     * signal EOF upstream although there is none downstream. Useful
     * to control an unbounded 'fcopy' for example, either through counting
     * bytes, or by pattern matching.
     */

    if (trans->in.vectors->maxReadProc == (Trf_QueryMaxRead*) NULL)
      maxRead = -1;
    else
      maxRead = trans->in.vectors->maxReadProc (trans->in.control,
						trans->clientData);

    if (maxRead >= 0) {
      if (maxRead < toRead) {
	toRead = maxRead;
      }
    } /* else: 'maxRead < 0' == Accept the current value of toRead */

    if (toRead <= 0) {
      PRINT ("Got %d, constrained by script\n", gotBytes); FL;
      DONE  (TrfInput);
      return gotBytes;
    }

    PRINT ("Read from parent %p\n", parent);
    IN; IN;

    read = READ (trans, buf, toRead);

    OT; OT;
    PRINT  ("................\n");
    /*PRTSTR ("Retrieved = {%d, \"%s\"}\n", read, buf);*/

    PRINT ("Retrieved = %d {\n", read);
    DUMP  (read, buf);
    PRINT ("}\n");
    STREAM_IN (trans, read, buf);

    if (read < 0) {
      /* Report errors to caller.
       * The state of the seek system is unchanged!
       */

      if ((Tcl_GetErrno () == EAGAIN) && (gotBytes > 0)) {
	  /* EAGAIN is a special situation.  If we had some data
	   * before we report that instead of the request to re-try.
	   */

	  PRINT ("Got %d, read < 0, <EAGAIN>\n", gotBytes);
	  FL; DONE (TrfInput);
	  return gotBytes;
      }

      *errorCodePtr = Tcl_GetErrno ();

      PRINT ("Got %d, read < 0, report error %d\n", gotBytes, *errorCodePtr);
      FL; DONE (TrfInput);
      return -1;      
    }

    if (read == 0) {
      /* Check wether we hit on EOF in 'parent' or
       * not. If not differentiate between blocking and
       * non-blocking modes. In non-blocking mode we ran
       * temporarily out of data. Signal this to the caller
       * via EWOULDBLOCK and error return (-1). In the other
       * cases we simply return what we got and let the
       * caller wait for more. On the other hand, if we got
       * an EOF we have to convert and flush all waiting
       * partial data.
       */

      /* 04/13/1999 Fileevent patch from Matt Newman <matt@novadigm.com>
       */
      if (! Tcl_Eof (parent)) {
	/* The state of the seek system is unchanged! */

	if (gotBytes == 0 && trans->flags & CHANNEL_ASYNC) {
	  *errorCodePtr = EWOULDBLOCK;

	  PRINT ("Got %d, report EWOULDBLOCK\n", gotBytes); FL;
	  DONE (TrfInput);
	  return -1;
	} else {
	  PRINT ("(Got = %d || not async)\n", gotBytes); FL;
	  DONE (TrfInput);
	  return gotBytes;
	}
      } else {
	PRINT ("EOF in downstream channel\n"); FL;
	if (trans->readIsFlushed) {
	  /* The state of the seek system is unchanged! */
	  /* already flushed, nothing to do anymore */
	  PRINT ("Got %d, !read flushed\n", gotBytes); FL;
	  DONE (TrfInput);
	  return gotBytes;
	}

	/* Now this is a bit different. The partial data waiting is converted
	 * and returned. So the 'AheadOffset' changes despite the location
	 * downstream not changing at all. It is now the negative of its
	 * additive inverse modulo 'numBytesDown':
	 *	 -((-k)%n) == -((n-1)-k) == k+1-n.
	 */

	PRINT ("in_.flushproc\n"); FL;

	trans->readIsFlushed = 1;
	trans->lastStored    = 0;

	res = trans->in.vectors->flushProc (trans->in.control,
					    (Tcl_Interp*) NULL,
					    trans->clientData);
	if (trans->seekState.allowed &&
	    trans->seekState.used.numBytesDown > 1) {
	  trans->seekState.aheadOffset += -trans->seekState.used.numBytesDown;
	}

	SEEK_DUMP (TrfInput; AhdC<);

	if (ResultLength (&trans->result) == 0) {
	  /* we had nothing to flush */
	  PRINT ("Got %d, read flushed / no result\n", gotBytes); FL;
	  DONE (TrfInput);
	  return gotBytes;
	}
	continue; /* at: while (toRead > 0) */
      }
    } /* read == 0 */

    /* Transform the read chunk, which was not empty.
     * The transformation processes 'read + aheadOffset' bytes.
     * So UP_CONVERT (read+ahead) == #bytes produced == ResultLength!
     * And  (read+ahead) % #down == #bytes now waiting == new ahead.
     */

    SEEK_DUMP (TrfInput; Read<);
    trans->lastStored = 0;

    if (trans->in.vectors->convertBufProc){ 
      PRINT ("in_.convertbufproc\n"); FL;

      res = trans->in.vectors->convertBufProc (trans->in.control,
					       (unsigned char*) buf, read,
					       (Tcl_Interp*) NULL,
					       trans->clientData);
    } else {
      PRINT ("in_.convertproc\n"); FL;

      res = TCL_OK;
      for (i=0; i < read; i++) {
	res = trans->in.vectors->convertProc (trans->in.control, buf [i],
					      (Tcl_Interp*) NULL,
					      trans->clientData);
	if (res != TCL_OK) {
	  break;
	}
      }
    }

    if (res != TCL_OK) {
      *errorCodePtr = EINVAL;
      PRINT ("Got %d, report error in transform (EINVAL)\n", gotBytes); FL;
      DONE (TrfInput);
      return -1;
    }

    /* Assert: UP_CONVERT (read+ahead) == ResultLength! */

    trans->seekState.downLoc += read;

    if (trans->seekState.allowed) {
      trans->seekState.aheadOffset += (read % trans->seekState.used.numBytesDown);
      trans->seekState.aheadOffset %= trans->seekState.used.numBytesDown;
    }

  } /* while toRead > 0 */

  SEEK_DUMP (TrfInput; Loop<);

  PRINT ("Got %d, after loop\n", gotBytes); FL;
  DONE (TrfInput);
  return gotBytes;
}

/*
 *------------------------------------------------------*
 *
 *	TrfOutput --
 *
 *	------------------------------------------------*
 *	Called by the generic IO system to convert data
 *	waiting to be written.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As defined by the transformation.
 *
 *	Result:
 *		A transformed buffer.
 *
 *------------------------------------------------------*
 */

static int
TrfOutput (instanceData, buf, toWrite, errorCodePtr)
ClientData instanceData;
CONST84 char*      buf;
int        toWrite;
int*       errorCodePtr;
{
  TrfTransformationInstance* trans = (TrfTransformationInstance*) instanceData;
  int i, res;
  Tcl_Channel parent;

  START (TrfOutput);

  parent = DOWNC (trans);

  /* should assert (trans->mode & TCL_WRITABLE) */

  /*
   * transformation results are automatically written to
   * the parent channel ('PutDestination' was configured
   * as write procedure in 'AttachTransform').
   */

  if (toWrite == 0) {
    /* Nothing came in to write, ignore the call
     */

    PRINT ("Nothing to write\n"); FL; DONE (TrfOutput);
    return 0;
  }

  SEEK_DUMP (TrfOutput; Start);

  /* toWrite / seekState.used.numBytesTransform = #tuples converted.
   * toWrite % seekState.used.numBytesTransform = #Bytes waiting in the transform.
   */

  SeekSynchronize (trans, parent);

  SEEK_DUMP (TrfOutput; Syncd);

  trans->lastWritten = 0;

  if (trans->out.vectors->convertBufProc){ 
    PRINT ("out.convertbufproc\n"); FL;

    res = trans->out.vectors->convertBufProc (trans->out.control,
					      (unsigned char*) buf, toWrite,
					      (Tcl_Interp*) NULL,
					      trans->clientData);
  } else {
    PRINT ("out.convertproc\n"); FL;

    res = TCL_OK;
    for (i=0; i < toWrite; i++) {
      res = trans->out.vectors->convertProc (trans->out.control, buf [i],
					     (Tcl_Interp*) NULL,
					     trans->clientData);
      if (res != TCL_OK) {
	break;
      }
    }
  }

  if (res != TCL_OK) {
    *errorCodePtr = EINVAL;
    PRINT ("error EINVAL\n"); FL; DONE (TrfInput);
    return -1;
  }

  /* Update seek state to new location
   * Assert: lastWritten == TRF_DOWN_CONVERT (trans, toWrite)
   */

  trans->seekState.upLoc        += toWrite;
  trans->seekState.upBufStartLoc = trans->seekState.upLoc;
  trans->seekState.upBufEndLoc   = trans->seekState.upLoc;
  trans->seekState.downLoc      += trans->lastWritten;
  trans->lastWritten       = 0;

  SEEK_DUMP (TrfOutput; Done_);

  /* In the last statement above the integer division automatically
   * strips off the #bytes waiting in the transform.
   */

  PRINT ("Written: %d\n", toWrite); FL; DONE (TrfOutput);
  return toWrite;
}

/*
 *------------------------------------------------------*
 *
 *	TrfSeek --
 *
 *	------------------------------------------------*
 *	This procedure is called by the generic IO level
 *	to move the access point in a channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Moves the location at which the channel
 *		will be accessed in future operations.
 *		Flushes all transformation buffers, then
 *		forwards it to the underlying channel.
 *
 *	Result:
 *		-1 if failed, the new position if
 *		successful. An output argument contains
 *		the POSIX error code if an error
 *		occurred, or zero.
 *
 *------------------------------------------------------*
 */

static int
TrfSeek (instanceData, offset, mode, errorCodePtr)
ClientData instanceData;	/* The channel to manipulate */
long       offset;		/* Size of movement. */
int        mode;		/* How to move */
int*       errorCodePtr;	/* Location of error flag. */
{
  TrfTransformationInstance* trans = (TrfTransformationInstance*) instanceData;
  int         result;
  Tcl_Channel parent;
  int         newLoc;

  START (TrfSeek);
  PRINT ("(Mode = %d, Offset = %ld)\n", mode, offset); FL;

  parent = DOWNC (trans);

  /*
   * Several things to look at before deciding what to do.
   * Is it a tell request ?
   * Is the channel unseekable ?
   * If not, are we in pass-down mode ?
   * If not, check buffer boundaries, etc. before discarding buffers, etc.
   */

  if ((offset == 0) && (mode == SEEK_CUR)) {
    /* Tell location.
     */

    PRINT ("[Tell], Location = %d\n", trans->seekState.upLoc); FL;
    DONE (TrfSeek);
    return trans->seekState.upLoc;
  }

  if (!trans->seekState.allowed) {
    *errorCodePtr = EINVAL;

    PRINT ("[Unseekable]\n"); FL; DONE (TrfSeek);
    return -1;
  }

  /* Assert: seekState.allowed, numBytesDown > 0, numBytesTransform > 0 */

  if (trans->seekCfg.identity) {
    /* Pass down mode. Pass request and record the change. This is used after
     * restoration of constrained seek to force the usage of a new zero-point.
     */

    PRINT ("[Passing down]\n"); FL;

    SeekClearBuffer (trans, TCL_WRITABLE | TCL_READABLE);

    trans->seekState.changed = 1;

    result = SEEK (trans, offset, mode);
    *errorCodePtr = (result == -1) ? Tcl_GetErrno () : 0;

    SEEK_DUMP (TrfSeek; Pass<);
    DONE (TrfSeek);
    return result;
  }

  /* Constrained seeking, as specified by the transformation.
   */

  if (mode == SEEK_SET) {
    /* Convert and handle absolute from start as relative to current
     * location.
     */

    PRINT ("[Seek from start] => Seek relative\n"); FL;
    result = TrfSeek (trans, offset - trans->seekState.upLoc, SEEK_CUR,
		      errorCodePtr);
    DONE (TrfSeek);
    return result;
  }

  if (mode == SEEK_END) {
    /* Can't do that right now! TODO */
    *errorCodePtr = EINVAL;

    PRINT ("[Seek from end not available]"); FL; DONE (TrfSeek);
    return -1;
  }

  /* Seeking relative to the current location.
   */

  newLoc = trans->seekState.upLoc + offset;

  if (newLoc % trans->seekState.used.numBytesTransform) {
    /* Seek allowed only to locations which are multiples of the input.
     */

    *errorCodePtr = EINVAL;

    PRINT ("Seek constrained to multiples of input tuples\n"); FL;
    DONE (TrfSeek);
    return -1;
  }

  if (newLoc < 0) {
    *errorCodePtr = EINVAL;

    PRINT ("[Seek relative], cannot seek before start of stream\n"); FL;
    DONE (TrfSeek);
    return -1;
  }

  if ((newLoc < trans->seekState.upBufStartLoc) ||
      (trans->seekState.upBufEndLoc <= newLoc)) {
    /* We are seeking out of the read buffer.
     * Discard it, adjust our position and seek the channel below to the
     * equivalent position.
     */

    int offsetDown, newDownLoc;

    PRINT ("[Seek relative], beyond read buffer\n"); FL;

    newDownLoc = trans->seekState.downZero + TRF_DOWN_CONVERT (trans, newLoc);
    offsetDown = newDownLoc - trans->seekState.downLoc;

    SeekClearBuffer (trans, TCL_WRITABLE | TCL_READABLE);

    if (offsetDown != 0) {
      result = SEEK (trans, offsetDown, SEEK_CUR);
      *errorCodePtr = (result == -1) ? Tcl_GetErrno () : 0;
    }

    trans->seekState.downLoc      += offsetDown;
    trans->seekState.upLoc         = newLoc;
    trans->seekState.upBufStartLoc = newLoc;
    trans->seekState.upBufEndLoc   = newLoc;

    SEEK_DUMP (TrfSeek; NoBuf);
    DONE (TrfSeek);
    return newLoc;
  }

  /* We are still inside the buffer, adjust the position
   * and clear out incomplete data waiting in the write
   * buffers, they are now invalid.
   */

  SeekClearBuffer (trans, TCL_WRITABLE);
  trans->seekState.upLoc = newLoc;

  SEEK_DUMP (TrfSeek; Base_);
  DONE (TrfSeek);
  return newLoc;
}

/*
 *------------------------------------------------------*
 *
 *	TrfWatch --
 *
 *	------------------------------------------------*
 *	Initialize the notifier to watch Tcl_Files from
 *	this channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Sets up the notifier so that a future
 *		event on the channel will be seen by Tcl.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */
	/* ARGSUSED */
static void
TrfWatch (instanceData, mask)
ClientData instanceData;	/* Channel to watch */
int        mask;		/* Events of interest */
{
  /*
   * 08/01/2000 - Completely rewritten to support as many versions of
   * the core and their different implementation s of stacked channels.
   */
  /* 04/13/1999 Fileevent patch from Matt Newman <matt@novadigm.com>
   * Added the comments.  */
  /* The caller expressed interest in events occuring for this
   * channel. Instead of forwarding the call to the underlying
   * channel we now express our interest in events on that
   * channel. This will ripple through all stacked channels to
   * the bottom-most real one actually able to generate events
   * (files, sockets, pipes, ...). The improvement beyond the
   * simple forwarding is that the generated events will ripple
   * back up to us, until they reach the channel the user
   * expressed his interest in (via fileevent). This way the
   * low-level events are propagated upward to the place where
   * the real event script resides, something which does not
   * happen in the simple forwarding model. It loses these events.
   */

  TrfTransformationInstance* trans = (TrfTransformationInstance*) instanceData;

  START (TrfWatch);

#ifndef USE_TCL_STUBS
  /* 8.0.x. Original patch. */

  if (mask == trans->watchMask) {
    /* No changes in the expressed interest, skip this call.
     */
    DONE (TrfWatch);
    return;
  }

  ChannelHandlerKS (trans, mask);
#else
  /* 8.1. and up */

  if ((trans->patchVariant == PATCH_ORIG) ||
      (trans->patchVariant == PATCH_82)) {

    if (mask == trans->watchMask) {
      /* No changes in the expressed interest, skip this call.
       */
      DONE (TrfWatch);
      return;
    }

    ChannelHandlerKS (trans, mask);

  } else if (trans->patchVariant == PATCH_832) {
    /* 8.3.2 and up */

    Tcl_DriverWatchProc* watchProc;
    Tcl_Channel          parent;

    trans->watchMask = mask;

    /* No channel handlers any more. We will be notified automatically
     * about events on the channel below via a call to our
     * 'TransformNotifyProc'. But we have to pass the interest down now.
     * We are allowed to add additional 'interest' to the mask if we want
     * to. But this transformation has no such interest. It just passes
     * the request down, unchanged.
     */

    parent    = DOWNC (trans);
    watchProc = Tcl_ChannelWatchProc (Tcl_GetChannelType (parent));

    (*watchProc) (Tcl_GetChannelInstanceData(parent), mask);

  } else {
    Tcl_Panic ("Illegal value for 'patchVariant'");
  }
#endif

  /*
   * Management of the internal timer.
   */

  if (!(mask & TCL_READABLE) || (ResultLength(&trans->result) == 0)) {
    /* A pending timer may exist, but either is there no (more)
     * interest in the events it generates or nothing is available
     * for reading. Remove it, if existing.
     */

    TimerKill (trans);
  } else {
    /* There might be no pending timer, but there is interest in
     * readable events and we actually have data waiting, so
     * generate a timer to flush that if it does not exist.
     */

    TimerSetup (trans);
  }

  DONE (TrfWatch);
}

/*
 *------------------------------------------------------*
 *
 *	TrfGetFile --
 *
 *	------------------------------------------------*
 *	Called from Tcl_GetChannelHandle to retrieve
 *	OS specific file handle from inside this channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		The appropriate Tcl_File or NULL if not
 *		present. 
 *
 *------------------------------------------------------*
 */

static int
TrfGetFile (instanceData, direction, handlePtr)
ClientData  instanceData;	/* Channel to query */
int         direction;		/* Direction of interest */
ClientData* handlePtr;		/* Place to store the handle into */
{
  /*
   * return handle belonging to parent channel
   */

  TrfTransformationInstance* trans = (TrfTransformationInstance*) instanceData;
  Tcl_Channel parent;

  START (TrfGetFile);

  parent = DOWNC (trans);

  DONE (TrfGetFile);
  return Tcl_GetChannelHandle (parent, direction, handlePtr);
}

/*
 *------------------------------------------------------*
 *
 *	TrfSetOption --
 *
 *	------------------------------------------------*
 *	Called by the generic layer to handle the reconfi-
 *	guration of channel specific options. Unknown
 *	options are passed downstream.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As defined by the channel downstream.
 *
 *	Result:
 *		A standard TCL error code.
 *
 *------------------------------------------------------*
 */

static int
TrfSetOption (instanceData, interp, optionName, value)
     ClientData  instanceData;
     Tcl_Interp* interp;
     CONST char* optionName;
     CONST char* value;
{
  /* Recognized options:
   *
   * -seekpolicy	Accepted values: unseekable, identity, {}
   */

  TrfTransformationInstance* trans = (TrfTransformationInstance*) instanceData;

  START (TrfSetOption);

  if (0 == strcmp (optionName, "-seekpolicy")) {
    /* The seekpolicy is about to be changed. Make sure that we got a valid
     * value and that it really changes the used policy. Failing the first
     * test causes an error, failing the second causes the system to silently
     * ignore this request. Reconfiguration will fail for a non-overidable
     * policy too.
     */

    if (!trans->seekCfg.overideAllowed) {
      Tcl_SetErrno (EINVAL);
      Tcl_AppendResult (interp, "It is not allowed to overide ",
			"the seek policy used by this channel.", NULL);
      DONE (TrfSetOption);
      return TCL_ERROR;
    }

    if (0 == strcmp (value, "unseekable")) {
      if (!trans->seekState.allowed) {
	/* Ignore the request if the channel already uses this policy.
	 */
	DONE (TrfSetOption);
	return TCL_OK;
      }

      TRF_SET_UNSEEKABLE (trans->seekState.used);
      trans->seekState.allowed = 0;
      trans->seekCfg.identity  = 0;

      /* Changed is not touched! We might have been forced to identity
       * before, and have to remember this for any restoration.
       */

    } else if (0 == strcmp (value, "identity")) {

      if (trans->seekState.allowed &&
	  (trans->seekState.used.numBytesTransform == 1) &&
	  (trans->seekState.used.numBytesDown == 1)) {

	/* Ignore the request if the channel already uses this policy.
	 */
	DONE (TrfSetOption);
	return TCL_OK;
      }

      trans->seekState.used.numBytesTransform = 1;
      trans->seekState.used.numBytesDown      = 1;
      trans->seekState.allowed                = 1;
      trans->seekCfg.identity                 = 1;
      trans->seekState.changed                = 0;

    } else if (0 == strcmp (value, "")) {
      if ((trans->seekState.used.numBytesTransform ==
	   trans->seekCfg.chosen.numBytesTransform) &&
	  (trans->seekState.used.numBytesDown ==
	   trans->seekCfg.chosen.numBytesDown)) {
	/* Ignore the request if the channel already uses hios chosen policy.
	 */
	DONE (TrfSetOption);
	return TCL_OK;
      }

      trans->seekState.used.numBytesTransform =
	trans->seekCfg.chosen.numBytesTransform;

      trans->seekState.used.numBytesDown =
	trans->seekCfg.chosen.numBytesDown;

      trans->seekState.allowed = !TRF_IS_UNSEEKABLE (trans->seekState.used);

      if (trans->seekState.changed) {
	/* Define new base location. Resync up and down to get the
	 * proper location without read-ahead. Reinitialize the
	 * upper location.
	 */

	Tcl_Channel parent = DOWNC (trans);
	SeekSynchronize (trans, parent);
	trans->seekState.downLoc     = TELL (trans);

#ifdef USE_TCL_STUBS
	if (trans->patchVariant == PATCH_832) {
	  trans->seekState.downLoc  -= Tcl_ChannelBuffered (parent);
	}
#endif
	trans->seekState.downZero    = trans->seekState.downLoc;
	trans->seekState.aheadOffset = 0;

	trans->seekState.upLoc         = 0;
	trans->seekState.upBufStartLoc = 0;
	trans->seekState.upBufEndLoc   = ResultLength (&trans->result);
      }

      trans->seekCfg.identity  = 0;
      trans->seekState.changed = 0;

    } else {
      Tcl_SetErrno (EINVAL);
      Tcl_AppendResult (interp, "Invalid value \"", value,
			"\", must be one of 'unseekable', 'identity' or ''.",
			NULL);
      DONE (TrfSetOption);
      return TCL_ERROR;
    }

  } else {
    int res;
    res = SETOPT (interp, trans, optionName, value);
    DONE (TrfSetOption);
    return res;
  }

  DONE (TrfSetOption);
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	TrfGetOption --
 *
 *	------------------------------------------------*
 *	Called by generic layer to handle requests for
 *	the values of channel specific options. As this
 *	channel type does not have such, it simply passes
 *	all requests downstream.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Adds characters to the DString refered by
 *		'dsPtr'.
 *
 *	Result:
 *		A standard TCL error code.
 *
 *------------------------------------------------------*
 */

static int
TrfGetOption (instanceData, interp, optionName, dsPtr)
     ClientData    instanceData;
     Tcl_Interp*   interp;
     CONST84 char* optionName;
     Tcl_DString*  dsPtr;
{
  /* Recognized options:
   *
   * -seekcfg
   * -seekstate
   * -seekpolicy
   */

  TrfTransformationInstance* trans = (TrfTransformationInstance*) instanceData;

  if (optionName == (char*) NULL) {
    /* A list of options and their values was requested,
     */

    Tcl_Obj* tmp;
    char policy [20];

    SeekPolicyGet (trans, policy);
    Tcl_DStringAppendElement (dsPtr, "-seekpolicy");
    Tcl_DStringAppendElement (dsPtr, policy);

    Tcl_DStringAppendElement (dsPtr, "-seekcfg");
    tmp = SeekConfigGet (interp, &trans->seekCfg);
    Tcl_DStringAppendElement (dsPtr, Tcl_GetStringFromObj (tmp, NULL));
    Tcl_DecrRefCount (tmp);

    Tcl_DStringAppendElement (dsPtr, "-seekstate");
    tmp = SeekStateGet (interp, &trans->seekState);
    Tcl_DStringAppendElement (dsPtr, Tcl_GetStringFromObj (tmp, NULL));
    Tcl_DecrRefCount (tmp);

    /* Pass the request down to all channels below so that we may a complete
     * state.
     */

    return GETOPT (interp, trans, optionName, dsPtr);

  } else if (0 == strcmp (optionName, "-seekpolicy")) {
    /* Deduce the policy in effect, use chosen/used
     * policy and identity to do this. Use a helper
     * procedure to allow easy reuse in the code above.
     */

    char policy [20];

    SeekPolicyGet (trans, policy);
    Tcl_DStringAppend (dsPtr, policy, -1);
    return TCL_OK;

  } else if (0 == strcmp (optionName, "-seekcfg")) {
    Tcl_Obj* tmp;

    tmp = SeekConfigGet (interp, &trans->seekCfg);
    Tcl_DStringAppend (dsPtr, Tcl_GetStringFromObj (tmp, NULL), -1);
    Tcl_DecrRefCount (tmp);

    return TCL_OK;
  } else if (0 == strcmp (optionName, "-seekstate")) {
    Tcl_Obj* tmp;

    tmp = SeekStateGet (interp, &trans->seekState);
    Tcl_DStringAppend (dsPtr, Tcl_GetStringFromObj (tmp, NULL), -1);
    Tcl_DecrRefCount (tmp);

    return TCL_OK;
  } else {
    /* Unknown option. Pass it down to the channels below, maybe one
     * of them is able to handle this request.
     */

    return GETOPT (interp, trans, optionName, dsPtr);
#if 0
    Tcl_SetErrno (EINVAL);
    return Tcl_BadChannelOption (interp, optionName, "seekcfg seekstate");
#endif
  }
}

#ifdef USE_TCL_STUBS
/*
 *------------------------------------------------------*
 *
 *	TrfNotify --
 *
 *	------------------------------------------------*
 *	Called by the generic layer of 8.3.2 and higher
 *	to handle events coming from below. We simply pass
 *	them upward.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		The unchanged interest mask.
 *
 *------------------------------------------------------*
 */
static int
TrfNotify (instanceData, interestMask)
     ClientData instanceData;
     int        interestMask;
{
  /*
   * An event occured in the underlying channel.  This transformation
   * doesn't process such events thus returns the incoming mask
   * unchanged.
   *
   * We do delete an existing timer. It was not fired, yet we are
   * here, so the channel below generated such an event and we don't
   * have to. The renewal of the interest after the execution of
   * channel handlers will eventually cause us to recreate the timer
   * (in TrfWatch).
   */

  TimerKill ((TrfTransformationInstance*) instanceData);
  return interestMask;
}
#endif

/*
 *------------------------------------------------------*
 *
 *	TransformImmediate --
 *
 *	------------------------------------------------*
 *	Read from source, apply the specified transformation
 *	and write the result to destination.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		The access points of source and destination
 *		change, data is added to destination too.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------* */

static int
TransformImmediate (interp, entry, source, destination, in, optInfo)
Tcl_Interp*        interp;
Trf_RegistryEntry* entry;
Tcl_Channel        source;
Tcl_Channel        destination;
struct Tcl_Obj* CONST in;
Trf_Options        optInfo;
{
  Trf_Vectors*     v;
  Trf_ControlBlock control;
  int              res = TCL_OK;

  ResultBuffer r;

  START (TransformImmediate);

  if (ENCODE_REQUEST (entry, optInfo)) {
    v = &(entry->trfType->encoder);
  } else {
    v = &(entry->trfType->decoder);
  }

  /* Take care of output (channel vs. interpreter result area).
   */

  if (destination == (Tcl_Channel) NULL) {
    ResultInit (&r);

    PRINT ("___.createproc\n"); FL;
    control = v->createProc ((ClientData) &r, PutInterpResult,
			     optInfo, interp,
			     entry->trfType->clientData);
  } else {
    PRINT ("___.createproc\n"); FL;
    control = v->createProc ((ClientData) destination, PutDestinationImm,
			     optInfo, interp,
			     entry->trfType->clientData);
  }

  if (control == (Trf_ControlBlock) NULL) {
    DONE (TransformImmediate);
    return TCL_ERROR;
  }


  /* Now differentiate between immediate value and channel as input.
   */

  if (source == (Tcl_Channel) NULL) {
    /* Immediate value.
     * -- VERSION DEPENDENT CODE --
     */
    int            length;
    unsigned char* buf;

    buf = GET_DATA (in, &length);
    if (v->convertBufProc) {
      /* play it safe, use a copy, avoid clobbering the input. */
      unsigned char* tmp;

      tmp = (unsigned char*) Tcl_Alloc (length);
      memcpy (tmp, buf, length);

      PRINT ("___.convertbufproc\n"); FL;

      res = v->convertBufProc (control, tmp, length, interp,
			       entry->trfType->clientData);
      Tcl_Free ((char*) tmp);
    } else {
      unsigned int i, c;
      
      PRINT ("___.convertproc\n"); FL;

      for (i=0; i < ((unsigned int) length); i++) {
	c = buf [i];
	res = v->convertProc (control, c, interp,
			      entry->trfType->clientData);
	
	if (res != TCL_OK)
	  break;
      }
    }

    if (res == TCL_OK) {
      PRINT ("___.flushproc\n"); FL;

      res = v->flushProc (control, interp, entry->trfType->clientData);
    }
  } else {
    /* Read from channel.
     */

    unsigned char* buf;
    int            actuallyRead;

    buf = (unsigned char*) Tcl_Alloc (READ_CHUNK_SIZE);

    while (1) {
      if (Tcl_Eof (source))
	break;

      actuallyRead = Tcl_Read (source, (char*) buf, READ_CHUNK_SIZE);

      if (actuallyRead <= 0)
	break;

      if (v->convertBufProc) {
	PRINT ("___.convertbufproc\n"); FL;

	res = v->convertBufProc (control, buf, actuallyRead, interp,
				 entry->trfType->clientData);
      } else {
	unsigned int i, c;

	PRINT ("___.convertproc\n"); FL;

	for (i=0; i < ((unsigned int) actuallyRead); i++) {
	  c = buf [i];
	  res = v->convertProc (control, c, interp,
				entry->trfType->clientData);
	  
	  if (res != TCL_OK)
	    break;
	}
      }

      if (res != TCL_OK)
	break;
    }

    Tcl_Free ((char*) buf);

    if (res == TCL_OK)
      res = v->flushProc (control, interp, entry->trfType->clientData);
  }

  PRINT ("___.deleteproc\n"); FL;
  v->deleteProc (control, entry->trfType->clientData);


  if (destination == (Tcl_Channel) NULL) {
    /* Now write into interpreter result area.
     */

    if (res == TCL_OK) {
      Tcl_ResetResult (interp);

      if (r.buf != NULL) {
	Tcl_Obj* o = NEW_DATA (r);
	Tcl_IncrRefCount (o);
	Tcl_SetObjResult (interp, o);
	Tcl_DecrRefCount (o);
      }
    }
    ResultClear (&r);
  }

  DONE (TransformImmediate);
  return res;
}

/*
 *------------------------------------------------------*
 *
 *	AttachTransform --
 *
 *	------------------------------------------------*
 *	Create an instance of a transformation and
 *	associate as filter it with the specified channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Allocates memory, changes the internal
 *		state of the channel.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
AttachTransform (entry, baseOpt, optInfo, interp)
Trf_RegistryEntry* entry;
Trf_BaseOptions*   baseOpt;
Trf_Options        optInfo;
Tcl_Interp*        interp;
{
  TrfTransformationInstance* trans;

  trans = NEW_TRANSFORM;

  START (AttachTransform);

#ifdef TRF_STREAM_DEBUG
  trans->inCounter  = 0;
  trans->outCounter = 0;
  trans->name       = (char*) entry->trfType->name;
#endif
#ifdef USE_TCL_STUBS
  trans->patchVariant = entry->registry->patchVariant;
#endif

  /* trans->standard.typePtr = entry->transType; */
  trans->clientData       = entry->trfType->clientData;

  if (trans->patchVariant == PATCH_832) {
    trans->parent = Tcl_GetTopChannel (baseOpt->attach);
  } else {
    trans->parent = baseOpt->attach;
  }

  trans->readIsFlushed    = 0;

  /* 04/13/1999 Fileevent patch from Matt Newman <matt@novadigm.com>
   */
  trans->flags            = 0;
  trans->watchMask        = 0;

  /* 03/28/2000 Added by DNew@Invisible.Net because Purify says so. */
  trans->lastStored       = 0;

  trans->mode             = Tcl_GetChannelMode (baseOpt->attach);
  trans->timer            = (Tcl_TimerToken) NULL;

  if (ENCODE_REQUEST (entry, optInfo)) {
    /* ENCODE on write
     * DECODE on read
     */

    trans->out.vectors = ((trans->mode & TCL_WRITABLE) ?
			  &entry->trfType->encoder     :
			  NULL);
    trans->in.vectors  = ((trans->mode & TCL_READABLE) ?
			  &entry->trfType->decoder     :
			  NULL);

  } else /* mode == DECODE */ {
    /* DECODE on write
     * ENCODE on read
     */

    trans->out.vectors = ((trans->mode & TCL_WRITABLE) ?
			  &entry->trfType->decoder     :
			  NULL);
    trans->in.vectors  = ((trans->mode & TCL_READABLE) ?
			  &entry->trfType->encoder     :
			  NULL);
  }

  /* 'PutDestination' is ok for write, only read
   * requires 'PutTrans' and its internal buffer.
   */

  if (trans->mode & TCL_WRITABLE) {
    PRINT ("out.createproc\n"); FL;

    trans->out.control = trans->out.vectors->createProc ((ClientData) trans,
							 PutDestination,
							 optInfo, interp,
							 trans->clientData);
  
    if (trans->out.control == (Trf_ControlBlock) NULL) {
      Tcl_Free ((char*) trans);
      DONE (AttachTransform);
      return TCL_ERROR;
    }
  }

  if (trans->mode & TCL_READABLE) {
    PRINT ("in_.createproc\n"); FL;

    trans->in.control  = trans->in.vectors->createProc  ((ClientData) trans,
							 PutTrans,
							 optInfo, interp,
							 trans->clientData);

    if (trans->in.control == (Trf_ControlBlock) NULL) {
      Tcl_Free ((char*) trans);
      DONE (AttachTransform);
      return TCL_ERROR;
    }
  }

  ResultInit (&trans->result);
  trans->result.seekState = &trans->seekState;

  /*
   * Build channel from converter definition and stack it upon the one we
   * shall attach to.
   */

  /* Discard information dangerous for the integrated patch.
   * (This makes sure that we don't miss any place using this pointer
   * without generating a crash (instead of some silent failure, like
   * thrashing far away memory)).
   */

#ifndef USE_TCL_STUBS
  trans->self   = Tcl_StackChannel (interp, entry->transType,
				    (ClientData) trans, trans->mode,
				    trans->parent);
#else
  if ((trans->patchVariant == PATCH_ORIG) ||
      (trans->patchVariant == PATCH_832)) {

    trans->self = Tcl_StackChannel (interp, entry->transType,
				    (ClientData) trans, trans->mode,
				    trans->parent);

  } else if (trans->patchVariant == PATCH_82) {
    trans->parent = NULL;
    trans->self   = baseOpt->attach;

    Tcl_StackChannel (interp, entry->transType,
		      (ClientData) trans, trans->mode,
		      trans->self);
  } else {
    Tcl_Panic ("Illegal value for 'patchVariant'");
  }
#endif

  if (trans->self == (Tcl_Channel) NULL) {
    Tcl_Free ((char*) trans);
    Tcl_AppendResult (interp, "internal error in Tcl_StackChannel",
		      (char*) NULL);
    DONE (AttachTransform);
    return TCL_ERROR;
  }

  /* Initialize the seek subsystem.
   */

  PRINTLN ("Initialize Seeking");
  PRINTLN ("Copy configuration");

  trans->seekCfg.natural.numBytesTransform =
    entry->trfType->naturalSeek.numBytesTransform;

  trans->seekCfg.natural.numBytesDown      =
    entry->trfType->naturalSeek.numBytesDown;

  if (optInfo && (*OPT->seekQueryProc != (Trf_SeekQueryOptions*) NULL)) {
    PRINTLN ("Query seekQueryProc");
    (*OPT->seekQueryProc) (interp, optInfo, &trans->seekCfg.natural, CLT);
  }

  PRINTLN ("Determine Policy");
  SeekCalculatePolicies (trans);

  PRINTLN ("    Initialize");
  SeekInitialize        (trans);

  /* Check for options overiding the policy. If they do despite being not
   * allowed to do so we have to remove the transformation and break it down.
   * We do this by calling 'Unstack', which does all the necessary things for
   * us.
   */

  PRINTLN ("    Policy options ?");
  if (baseOpt->policy != (Tcl_Obj*) NULL) {
    if (TCL_OK != TrfSetOption ((ClientData) trans, interp, "-seekpolicy",
				Tcl_GetStringFromObj (baseOpt->policy,
						      NULL))) {

      /* An error prevented setting a policy. Save the resulting error
       * message across the necessary unstacking of the now faulty
       * transformation.
       */

#if GT81
      Tcl_SavedResult ciSave;

      Tcl_SaveResult     (interp, &ciSave);
      Tcl_UnstackChannel (interp, trans->self);
      Tcl_RestoreResult  (interp, &ciSave);
#else
      Tcl_UnstackChannel (interp, trans->self);
#endif
      DONE (AttachTransform);
      return TCL_ERROR;
    }
  }

  /*  Tcl_RegisterChannel (interp, new); */
  Tcl_AppendResult (interp, Tcl_GetChannelName (trans->self),
		    (char*) NULL);
  DONE (AttachTransform);
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	PutDestination --
 *
 *	------------------------------------------------*
 *	Handler used by a transformation to write its results.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Writes to the channel.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
PutDestination (clientData, outString, outLen, interp)
ClientData     clientData;
unsigned char* outString;
int            outLen;
Tcl_Interp*    interp;
{
  TrfTransformationInstance* trans = (TrfTransformationInstance*) clientData;
  int         res;
  Tcl_Channel parent;

  START  (PutDestination);
  /*PRTSTR ("Data = {%d, \"%s\"}\n", outLen, outString);*/
  PRINT ("Data = %d {\n", outLen);
  DUMP  (outLen, outString);
  PRINT ("}\n");

  parent = DOWNC (trans);

  trans->lastWritten += outLen;

  res = WRITE (trans, (char*) outString, outLen);

  if (res < 0) {
    if (interp) {
      Tcl_AppendResult (interp, "error writing \"",
			Tcl_GetChannelName (parent),
			"\": ", Tcl_PosixError (interp),
			(char*) NULL);
    }
    PRINT ("ERROR /written = %d, errno = %d, (%d) %s\n",
	   res, Tcl_GetErrno (), EACCES, strerror (Tcl_GetErrno ()));
    DONE (PutDestination);
    return TCL_ERROR;
  }

  DONE (PutDestination);
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	PutDestinationImm --
 *
 *	------------------------------------------------*
 *	Handler used during an immediate transformation
 *	to write its results into the -out channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		Writes to the channel.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
PutDestinationImm (clientData, outString, outLen, interp)
ClientData     clientData;
unsigned char* outString;
int            outLen;
Tcl_Interp*    interp;
{
  int         res;
  Tcl_Channel destination = (Tcl_Channel) clientData;

  START  (PutDestinationImm);
  /*PRTSTR ("Data = {%d, \"%s\"}\n", outLen, outString);*/
  PRINT ("Data = %d {\n", outLen);
  DUMP  (outLen, outString);
  PRINT ("}\n");

  res = Tcl_Write (destination, (char*) outString, outLen);

  if (res < 0) {
    if (interp) {
      Tcl_AppendResult (interp, "error writing \"",
			Tcl_GetChannelName (destination),
			"\": ", Tcl_PosixError (interp),
			(char*) NULL);
    }
    DONE (PutDestinationImm);
    return TCL_ERROR;
  }

  DONE (PutDestinationImm);
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	PutTrans --
 *
 *	------------------------------------------------*
 *	Handler used by a transformation to write its
 *	results (to be read later). Used by transformations
 *	acting as filter.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		May allocate memory.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
PutTrans (clientData, outString, outLen, interp)
ClientData     clientData;
unsigned char* outString;
int            outLen;
Tcl_Interp*    interp;
{
  TrfTransformationInstance* trans = (TrfTransformationInstance*) clientData;

  START  (PutTrans);
  /*PRTSTR ("Data = {%d, \"%s\"}\n", outLen, outString);*/
  PRINT ("Data = %d {\n", outLen);
  DUMP  (outLen, outString);
  PRINT ("}\n");
  STREAM_OUT (trans, outLen, outString);

  trans->lastStored += outLen;

  ResultAdd (&trans->result, outString, outLen);

  DONE (PutTrans);
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	PutInterpResult --
 *
 *	------------------------------------------------*
 *	Handler used by a transformation to write its
 *	results into the interpreter result area.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		changes the contents of the interpreter
 *		result area.
 *
 *	Result:
 *		A standard Tcl error code.
 *
 *------------------------------------------------------*
 */

static int
PutInterpResult (clientData, outString, outLen, interp)
ClientData     clientData;
unsigned char* outString;
int            outLen;
Tcl_Interp*    interp;
{
  ResultBuffer* r = (ResultBuffer*) clientData;

  START  (PutInterpResult);
  /*PRTSTR ("Data = {%d, \"%s\"}\n", outLen, outString);*/
  PRINT ("Data = %d {\n", outLen);
  DUMP  (outLen, outString);
  PRINT ("}\n");

  ResultAdd (r, outString, outLen);

  DONE (PutInterpResult);
  return TCL_OK;
}

/* 04/13/1999 Fileevent patch from Matt Newman <matt@novadigm.com>
 */
/*
 *------------------------------------------------------*
 *
 *	ChannelHandler --
 *
 *	------------------------------------------------*
 *	Handler called by Tcl as a result of
 *	Tcl_CreateChannelHandler - to inform us of activity
 *	on the underlying channel.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		May generate subsequent calls to
 *		Tcl_NotifyChannel.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ChannelHandler (clientData, mask)
ClientData     clientData;
int            mask;
{
  /*
   * An event occured in the underlying channel. Forward it to
   * ourself. This will either execute an attached event script
   * (fileevent) or an intermediate handler like this one propagating
   * the event further upward.
   *
   * This procedure is called only for the original and the 8.2
   * patch. The 8.2.3 patch uses a new vector in the driver to get and
   * handle events coming from below.
   */

  TrfTransformationInstance* trans = (TrfTransformationInstance*) clientData;

#ifndef USE_TCL_STUBS
  /*
   * Core 8.0.x. Forward the event to ourselves.
   */

  Tcl_NotifyChannel (trans->self, mask);
#else
  /*
   * Check for the correct variants first. Forwarding the event is not
   * required for the 8.2 patch. For that variant the core,
   * i.e. Tcl_NotifyChannel loops over all channels in the stack by
   * itself.
   */

  if (trans->patchVariant == PATCH_832) {
    Tcl_Panic ("Illegal value for 'patchVariant' in ChannelHandler");
  }
  if (trans->patchVariant == PATCH_ORIG) {
    Tcl_NotifyChannel (trans->self, mask);
  }
#endif

  /*
   * Check the I/O-Buffers of this channel for waiting information.
   * Setup a timer generating an artificial event for us if we have
   * such. We could call Tcl_NotifyChannel directly, but this would
   * starve other event sources, so a timer is used to prevent that.
   */

  TimerKill (trans);

  /* Check for waiting data, flush it out with a timer.
   */

#ifndef USE_TCL_STUBS
  if ((mask & TCL_READABLE) && ((ResultLength (&trans->result) > 0) ||
				(Tcl_InputBuffered (trans->self) > 0))) {
    TimerSetup (trans);
  }
#else
  if (trans->patchVariant != PATCH_ORIG) {
    if ((mask & TCL_READABLE) && (ResultLength (&trans->result) > 0)) {
      TimerSetup (trans);
    }
  } else {
    if ((mask & TCL_READABLE) && ((ResultLength (&trans->result) > 0) ||
				  (Tcl_InputBuffered (trans->self) > 0))) {
      TimerSetup (trans);
    }
  }
#endif
}

/*
 *------------------------------------------------------*
 *
 *	ChannelHandlerTimer --
 *
 *	------------------------------------------------*
 *	Called by the notifier (-> timer) to flush out
 *	information waiting in channel buffers.
 *	------------------------------------------------*
 *
 *	Sideeffects:
 *		As of 'ChannelHandler'.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ChannelHandlerTimer (clientData)
ClientData clientData; /* Transformation to query */
{
  TrfTransformationInstance* trans = (TrfTransformationInstance*) clientData;

  trans->timer = (Tcl_TimerToken) NULL;

#ifndef USE_TCL_STUBS
  /* 8.0.x.
   * Use the channel handler itself to do the necessary actions
   */

  ChannelHandler (clientData, trans->watchMask);
#else
  if ((trans->patchVariant == PATCH_82) ||
      (trans->patchVariant == PATCH_832)) {
    /*
     * Use the standard notification mechanism to invoke all channel
     * handlers.
     */
    Tcl_NotifyChannel (trans->self, TCL_READABLE);
  } else {
    /* PATCH_ORIG, seee 8.0.x
     */

    ChannelHandler (clientData, trans->watchMask);
  }
#endif
}

#ifdef USE_TCL_STUBS
/*
 *------------------------------------------------------*
 *
 *	DownSOpt --
 *
 *	Helper procedure. Writes an option to the downstream channel.
 *
 *	Sideeffects:
 *		As of Tcl_SetChannelOption
 *
 *	Result:
 *		A standard tcl error code.
 *
 *------------------------------------------------------*
 */

static int
DownSOpt (interp, ctrl, optionName, value)
     Tcl_Interp*                interp;
     TrfTransformationInstance* ctrl;
     CONST char*                optionName;
     CONST char*                value;
{
  Tcl_Channel parent = DOWNC (ctrl);

  if (ctrl->patchVariant == PATCH_832) {
    /*
     * The newly written patch forces direct use of the driver.
     */

    Tcl_DriverSetOptionProc *setOptionProc = 
      Tcl_ChannelSetOptionProc (Tcl_GetChannelType (parent));

    if (setOptionProc != NULL) {
      return (*setOptionProc) (Tcl_GetChannelInstanceData (parent),
			       interp, optionName, value);
    } else {
      return TCL_ERROR;
    }

  } else {
    return Tcl_SetChannelOption (interp, parent, optionName, value);
  }
}

/*
 *------------------------------------------------------*
 *
 *	DownGOpt --
 *
 *	Helper procedure. Reads options from the downstream channel.
 *
 *	Sideeffects:
 *		As of Tcl_GetChannelOption
 *
 *	Result:
 *		A standard tcl error code.
 *
 *------------------------------------------------------*
 */

static int
DownGOpt (interp, ctrl, optionName, dsPtr)
     Tcl_Interp*                interp;
     TrfTransformationInstance* ctrl;
     CONST84 char*              optionName;
     Tcl_DString*               dsPtr;
{
  Tcl_Channel parent = DOWNC (ctrl);

  if (ctrl->patchVariant == PATCH_832) {
    /*
     * The newly written patch forces direct use of the driver.
     */

    Tcl_DriverGetOptionProc *getOptionProc =
      Tcl_ChannelGetOptionProc (Tcl_GetChannelType (parent));

    if (getOptionProc != NULL) {
	return (*getOptionProc) (Tcl_GetChannelInstanceData (parent),
				 interp, optionName, dsPtr);
    }

    /*
     * Downstream channel has no driver to get options. Fall back on
     * some default behaviour. A query for all options is ok. A
     * request for a specific unknown option OTOH has to fail.
     */

    if (optionName == (char*) NULL) {
      return TCL_OK;
    } else {
      return TCL_ERROR;
    }
  } else {
    return Tcl_GetChannelOption (interp, parent, optionName, dsPtr);
  }
}

/*
 *------------------------------------------------------*
 *
 *	DownWrite --
 *
 *	Helper procedure. Writes to the downstream channel.
 *
 *	Sideeffects:
 *		As of TclWrite / Tcl_WriteRaw
 *
 *	Result:
 *		The number of bytes written.
 *
 *------------------------------------------------------*
 */

static int
DownWrite (ctrl, buf, toWrite)
     TrfTransformationInstance* ctrl;
     char*                      buf;
     int                        toWrite;
{
  Tcl_Channel parent = DOWNC (ctrl);

  if (ctrl->patchVariant == PATCH_832) {
    /*
     * The newly written patch forces use of the new raw-API.
     */

    PRINT ("WriteRaw %p %s\n", parent, Tcl_GetChannelType (parent)->typeName);
    return Tcl_WriteRaw (parent, buf, toWrite);
  } else {
    return Tcl_Write (parent, buf, toWrite);
  }
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	DownRead --
 *
 *	Helper procedure. Reads from the downstream channel.
 *
 *	Sideeffects:
 *		As of TclRead / Tcl_ReadRaw
 *
 *	Result:
 *		The number of bytes read.
 *
 *------------------------------------------------------*
 */

static int
DownRead (ctrl, buf, toRead)
     TrfTransformationInstance* ctrl;
     char*                      buf;
     int                        toRead;
{
  Tcl_Channel parent = DOWNC (ctrl);

  if (ctrl->patchVariant == PATCH_832) {
    /*
     * The newly written patch forces use of the new raw-API.
     */

    return Tcl_ReadRaw (parent, buf, toRead);
  } else {
    return Tcl_Read (parent, buf, toRead);
  }
  return TCL_OK;
}

/*
 *------------------------------------------------------*
 *
 *	DownSeek --
 *
 *	Helper procedure. Asks the downstream channel
 *	to seek, or for its current location.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		The location in the downstream channel
 *
 *------------------------------------------------------*
 */

static int
DownSeek (ctrl, offset, mode)
    TrfTransformationInstance* ctrl;
    int                        offset;
    int                        mode;
{
  Tcl_Channel parent = DOWNC (ctrl);

  if (ctrl->patchVariant == PATCH_832) {
    /*
     * The newly rewritten patch forces the transformation into
     * directly using the seek-proc of the downstream driver. Tcl_Seek
     * would compensate for the stack and cause and infinite recursion
     * blowing the stack.
     */

    Tcl_ChannelType*    parentType     = Tcl_GetChannelType  (parent);
    Tcl_DriverSeekProc* parentSeekProc = Tcl_ChannelSeekProc (parentType);
    int                 errorCode;

    if (parentSeekProc == (Tcl_DriverSeekProc*) NULL) {
      return -1;
    }

    return (*parentSeekProc) (Tcl_GetChannelInstanceData (parent),
			      offset, mode, &errorCode);
  }

  /*
   * (ctrl->patchVariant == PATCH_ORIG)
   * (ctrl->patchVariant == PATCH_82)
   *
   * Both the original patch for stacked channels and rewritten
   * implementation for 8.2. have the same simple semantics for
   * getting at the location of the downstream channel.
   *
   * Just use the standard 'Tcl_Seek'.
   */

    return (int) Tcl_Seek (parent, offset, mode);
}

/*
 *------------------------------------------------------*
 *
 *	DownChannel --
 *
 *	Helper procedure. Finds the downstream channel.
 *
 *	Sideeffects:
 *		May modify 'self'.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static Tcl_Channel
DownChannel (ctrl)
    TrfTransformationInstance* ctrl;
{
  Tcl_Channel self;
  Tcl_Channel next;

  if ((ctrl->patchVariant == PATCH_ORIG) ||
      (ctrl->patchVariant == PATCH_832)) {
    /*
     * Both the original patch for stacked channels and rewritten
     * implementation for 8.3.2. have simple semantics for getting at
     * the parent of a channel.
     */

    return ctrl->parent;
  }

  /*
   * The first rewrite of the stacked channel patch initially included
   * in 8.2. requires that a transformation searches it's channel in
   * the whole stack. Only for the versions of the core using this
   * implementation, 8.2 till 8.3.1, the comments below apply.
   */

  /* The reason for the existence of this procedure is
   * the fact that stacking a transform over another
   * transform will leave our internal pointer unchanged,
   * and thus pointing to the new transform, and not the
   * Channel structure containing the saved state of this
   * transform. This is the price to pay for leaving
   * Tcl_Channel references intact. The only other solution
   * is an extension of Tcl_ChannelType with another driver
   * procedure to notify a Channel about the (un)stacking.
   *
   * It walks the chain of Channel structures until it
   * finds the one pointing having 'ctrl' as instanceData
   * and then returns the superceding channel to that.
   */

  self = ctrl->self;

  while ((ClientData) ctrl != Tcl_GetChannelInstanceData (self)) {
    next = Tcl_GetStackedChannel (self);
    if (next == (Tcl_Channel) NULL) {
      /* 09/24/1999 Unstacking bug, found by Matt Newman <matt@sensus.org>.
       *
       * We were unable to find the channel structure for this
       * transformation in the chain of stacked channel. This
       * means that we are currently in the process of unstacking
       * it *and* there were some bytes waiting which are now
       * flushed. In this situation the pointer to the channel
       * itself already refers to the parent channel we have to
       * write the bytes into, so we return that.
       */
      return ctrl->self;
    }
    self = next;
  }

  return Tcl_GetStackedChannel (self);
}
#endif 

/*
 *------------------------------------------------------*
 *
 *	ResultClear --
 *
 *	Deallocates any memory allocated by 'ResultAdd'.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ResultClear (r)
     ResultBuffer* r; /* Reference to the buffer to clear out */
{
  r->used = 0;

  if (r->allocated) {
    Tcl_Free ((char*) r->buf);
    r->buf       = (unsigned char*) NULL;
    r->allocated = 0;
  }

  if (r->seekState != (SeekState*) NULL) {
    r->seekState->upBufStartLoc  = r->seekState->upLoc;
    r->seekState->upBufEndLoc    = r->seekState->upLoc;
  }
}

/*
 *------------------------------------------------------*
 *
 *	ResultInit --
 *
 *	Initializes the specified buffer structure. The
 *	structure will contain valid information for an
 *	emtpy buffer.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ResultInit (r)
    ResultBuffer* r; /* Reference to the structure to initialize */
{
    r->used      = 0;
    r->allocated = 0;
    r->buf       = (unsigned char*) NULL;
    r->seekState = (SeekState*) NULL;
}

/*
 *------------------------------------------------------*
 *
 *	ResultLength --
 *
 *	Returns the number of bytes stored in the buffer.
 *
 *	Sideeffects:
 *		None.
 *
 *	Result:
 *		An integer, see above too.
 *
 *------------------------------------------------------*
 */

static int
ResultLength (r)
    ResultBuffer* r; /* The structure to query */
{
    return r->used;
}

/*
 *------------------------------------------------------*
 *
 *	ResultCopy --
 *
 *	Copies the requested number of bytes from the
 *	buffer into the specified array and removes them
 *	from the buffer afterward. Copies less if there
 *	is not enough data in the buffer.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		The number of actually copied bytes,
 *		possibly less than 'toRead'.
 *
 *------------------------------------------------------*
 */

static int
ResultCopy (r, buf, toRead)
     ResultBuffer*  r;      /* The buffer to read from */
     unsigned char* buf;    /* The buffer to copy into */
     int            toRead; /* Number of requested bytes */
{
  int copied;

  START (ResultCopy);
  PRINT ("request = %d, have = %d\n", toRead, r->used); FL;

  if (r->used == 0) {
    /* Nothing to copy in the case of an empty buffer.
     */

    copied = 0;
    goto done;
  }

  if (r->used == toRead) {
    /* We have just enough. Copy everything to the caller.
     */

    memcpy ((VOID*) buf, (VOID*) r->buf, toRead);
    r->used = 0;

    copied = toRead;
    goto done;
  }

  if (r->used > toRead) {
    /* The internal buffer contains more than requested.
     * Copy the requested subset to the caller, and shift
     * the remaining bytes down.
     */

    memcpy  ((VOID*) buf,    (VOID*) r->buf,            toRead);
    memmove ((VOID*) r->buf, (VOID*) (r->buf + toRead), r->used - toRead);

    r->used -= toRead;

    copied = toRead;
    goto done;
  }

  /* There is not enough in the buffer to satisfy the caller, so
   * take everything.
   */

  memcpy ((VOID*) buf, (VOID*) r->buf, r->used);
  toRead  = r->used;
  r->used = 0;
  copied  = toRead;

  /* -- common postwork code ------- */

done:
  if ((copied > 0) &&
      (r->seekState != (SeekState*) NULL)) {
    r->seekState->upBufStartLoc += copied;
  }

  DONE (ResultCopy);
  return copied;
}

/*
 *------------------------------------------------------*
 *
 *	ResultDiscardAtStart --
 *
 *	Removes the n bytes at the beginning of the buffer
 *	from it. Clears the buffer if n is greater than
 *	its length.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ResultDiscardAtStart (r, n)
     ResultBuffer*  r; /* The buffer to manipulate  */
     int            n; /* Number of bytes to remove */
{
  START (ResultDiscardAtStart);
  PRINT ("n = %d, have = %d\n", n, r->used); FL;

  if (r->used == 0) {
    /* Nothing to remove in the case of an empty buffer.
     */

    DONE (ResultDiscardAtStart);
    return;
  }

  if (n > r->used) {
    ResultClear (r);
    DONE (ResultDiscardAtStart);
    return;
  }

  /* Shift remaining information down */

  memmove ((VOID*) r->buf, (VOID*) (r->buf + n), r->used - n);
  r->used -= n;

  if (r->seekState != (SeekState*) NULL) {
    r->seekState->upBufStartLoc += n;
  }

  DONE (ResultCopy);
}

/*
 *------------------------------------------------------*
 *
 *	ResultAdd --
 *
 *	Adds the bytes in the specified array to the
 *	buffer, by appending it.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ResultAdd (r, buf, toWrite)
    ResultBuffer*  r;       /* The buffer to extend */
    unsigned char* buf;     /* The buffer to read from */
    int            toWrite; /* The number of bytes in 'buf' */
{
  START (ResultAdd);
  PRINT ("have %d, adding %d\n", r->used, toWrite); FL;

  if ((r->used + toWrite + 1) > r->allocated) {
    /* Extension of the internal buffer is required.
     */

    if (r->allocated == 0) {
      r->allocated = toWrite + INCREMENT;
      r->buf       = (unsigned char*) Tcl_Alloc (r->allocated);
    } else {
      r->allocated += toWrite + INCREMENT;
      r->buf        = (unsigned char*) Tcl_Realloc((char*) r->buf,
						   r->allocated);
    }
  }

  /* now copy data */
  memcpy (r->buf + r->used, buf, toWrite);
  r->used += toWrite;

  if (r->seekState != (SeekState*) NULL) {
    r->seekState->upBufEndLoc += toWrite;
  }

  DONE (ResultAdd);
}

/*
 *------------------------------------------------------*
 *
 *	SeekCalculatePolicies --
 *
 *	Computes standard and used policy from the natural
 *	policy of the transformation, all transformations
 *	below and its base channel.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
SeekCalculatePolicies (trans)
     TrfTransformationInstance* trans;
{
  /* Define seek related runtime configuration.
   * seekCfg.overideAllowed, seekCfg.chosen, seekState.used
   *
   * i.   some transformation below unseekable ? not-overidable unseekable
   * ii.  base channel unseekable ?              see above
   * iii. naturally unseekable ?                 overidable unseekable.
   *
   * WARNING: For 8.0 and 8.1 we will always return 'unseekable'. Due to a
   * missing 'Tcl_GetStackedChannel' we are unable to go down through the
   * stack of transformations.
   */

#ifndef USE_TCL_STUBS
  START (SeekCalculatePolicies);
  PRINTLN ("8.0., no Tcl_GetStackedChannel, unseekable, no overide");

  TRF_SET_UNSEEKABLE (trans->seekCfg.chosen);
  trans->seekCfg.overideAllowed = 0;

#else
  Tcl_Channel self = trans->self;
  Tcl_Channel next;

  int stopped = 0;

  START (SeekCalculatePolicies);

  if (trans->patchVariant == PATCH_ORIG) {
    PRINTLN ("8.1., no Tcl_GetStackedChannel, unseekable, no overide");

    TRF_SET_UNSEEKABLE (trans->seekCfg.chosen);
    trans->seekCfg.overideAllowed = 0;
    goto done;
  }

  /* 8.2 or higher */

  while (self != (Tcl_Channel) NULL) {
    PRINT ("Check %p\n", self); FL;

#if GT81
    next = Tcl_GetStackedChannel (self);
#else
    /* In case of 8.1 and higher we can use the (integrated or patched)
     * 'Tcl_GetStackedChannel' to find the next transform in a general
     * way. Else we have to check the type of 'next' itself before trying
     * to peek into its structure. If it is no Trf transform we cannot go
     * deeper into the stack. But that is not necessary, as the result of
     * 'unseekable' will not change anymore.
     */

    if (Tcl_GetChannelType (self)->seekProc != TrfSeek) {
      PRINTLN ("Can't go further down, unseekable, disallow overide");

      TRF_SET_UNSEEKABLE (trans->seekCfg.chosen);
      trans->seekCfg.overideAllowed = 0;
      stopped = 1;
      break;
    }

    next = ((TrfTransformationInstance*) 
		   Tcl_GetChannelInstanceData (self))->parent;
#endif

    if (next == (Tcl_Channel) NULL) {
      /* self points to base channel (ii).
       */

      if (Tcl_GetChannelType (self)->seekProc == (Tcl_DriverSeekProc*) NULL) {
	/* Base is unseekable.
	 */

	PRINTLN ("Base is unseekable");

	TRF_SET_UNSEEKABLE (trans->seekCfg.chosen);
	trans->seekCfg.overideAllowed = 0;
	stopped = 1;
	break;
      }
    } else {
      /* 'next' points to a transformation.
       */

      Tcl_Channel nextAfter;

#if GT81
      nextAfter = Tcl_GetStackedChannel (next);
#else      
      nextAfter = ((TrfTransformationInstance*) 
		   Tcl_GetChannelInstanceData (next))->parent;
#endif

      if (nextAfter != (Tcl_Channel) NULL) {
	/* next points to a transformation below the top (i).
	 * Assume unseekable for a non-trf transformation, else peek directly
	 * into the relevant structure
	 */

	if (Tcl_GetChannelType (next)->seekProc != TrfSeek) {
	  PRINTLN ("Unknown type of transform, unseekable, no overide");

	  TRF_SET_UNSEEKABLE (trans->seekCfg.chosen);
	  trans->seekCfg.overideAllowed = 0;
	  stopped = 1;
	} else {
	  TrfTransformationInstance* down = 
	    (TrfTransformationInstance*) Tcl_GetChannelInstanceData (next);
	  
	  if (!down->seekState.allowed) {
	    PRINTLN ("Trf transform, unseekable");

	    TRF_SET_UNSEEKABLE (trans->seekCfg.chosen);
	    trans->seekCfg.overideAllowed = 0;
	    stopped = 1;
	  }
	}
      } else {
	/* Next points to the base channel */
	/* assert (0); */
      }
    }

    self = next;
  }

  PRINTLN ("Looping done");

  if (!stopped) {
    PRINTLN ("Search went through, check natural policy");

    if (TRF_IS_UNSEEKABLE (trans->seekCfg.natural)) {
      /* Naturally unseekable (iii)
       */

      PRINTLN ("Naturally unseekable");

      TRF_SET_UNSEEKABLE (trans->seekCfg.chosen);
      trans->seekCfg.overideAllowed = 1;
    } else {
      /* Take the natural ratio.
       */

      PRINTLN ("naturally seekable");

      trans->seekCfg.chosen.numBytesTransform =
	trans->seekCfg.natural.numBytesTransform;

      trans->seekCfg.chosen.numBytesDown      =
	trans->seekCfg.natural.numBytesDown;

      trans->seekCfg.overideAllowed = 1;
    }
  }
#endif

  PRINTLN ("Copy ratio chosen :- used");

#ifdef USE_TCL_STUBS
done:
#endif
  trans->seekState.used.numBytesTransform =
    trans->seekCfg.chosen.numBytesTransform;

  trans->seekState.used.numBytesDown      =
    trans->seekCfg.chosen.numBytesDown;

  trans->seekState.allowed                =
    !TRF_IS_UNSEEKABLE (trans->seekState.used);

  DONE (SeekCalculatePolicies);
}

/*
 *------------------------------------------------------*
 *
 *	SeekInitialize --
 *
 *	Initialize the runtime state of the seek mechanisms
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
SeekInitialize (trans)
     TrfTransformationInstance* trans;
{
  trans->seekState.upLoc         = 0;
  trans->seekState.upBufStartLoc = 0;
  trans->seekState.upBufEndLoc   = 0;

  if (trans->seekState.allowed) {
    trans->seekState.downLoc     = TELL (trans);
#ifdef USE_TCL_STUBS
    if (trans->patchVariant == PATCH_832) {
      trans->seekState.downLoc  -= Tcl_ChannelBuffered (DOWNC (trans));
    }
#endif
    trans->seekState.downZero    = trans->seekState.downLoc;
    trans->seekState.aheadOffset = 0;
  } else {
    trans->seekState.downLoc     = 0;
    trans->seekState.downZero    = 0;
    trans->seekState.aheadOffset = 0;
  }

  trans->seekCfg.identity   = 0;
  trans->seekState.changed  = 0;

  SEEK_DUMP (Seek Initialized);
}

/*
 *------------------------------------------------------*
 *
 *	SeekClearBuffer --
 *
 *	Clear read / write buffers of the transformation,
 *	as specified by the second argument.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
SeekClearBuffer (trans, which)
     TrfTransformationInstance* trans;
     int                        which;
{
  /*
   * Discard everything in the input and output buffers, both
   * in the transformation and in the generic layer of Trf.
   */

  if (trans->mode & which & TCL_WRITABLE) {
    PRINT ("out.clearproc\n"); FL;

    trans->out.vectors->clearProc (trans->out.control, trans->clientData);
  }

  if (trans->mode & which & TCL_READABLE) {
    PRINT ("in.clearproc\n"); FL;

    trans->in.vectors->clearProc  (trans->in.control, trans->clientData);
    trans->readIsFlushed = 0;
    ResultClear (&trans->result);
  }
}

/*
 *------------------------------------------------------*
 *
 *	SeekSynchronize --
 *
 *	Discard an existing read buffer and annulate the
 *	read ahead in the downstream channel.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
SeekSynchronize (trans, parent)
     TrfTransformationInstance* trans;
     Tcl_Channel                parent;
{
  int offsetDown;

  if (!trans->seekState.allowed) {
    /* No synchronisation required for an unseekable transform */
    return;
  }

  if ((trans->seekState.upLoc == trans->seekState.upBufEndLoc) &&
      (trans->seekState.aheadOffset == 0)) {
    /* Up and down locations are in sync, nothing to do. */
    return;
  }

  PRINT ("in.clearproc\n"); FL;

  trans->in.vectors->clearProc  (trans->in.control, trans->clientData);
  trans->readIsFlushed = 0;

  offsetDown  = TRF_DOWN_CONVERT (trans,
				  trans->seekState.upLoc - trans->seekState.upBufEndLoc);
  offsetDown -= trans->seekState.aheadOffset; /* !! */

  ResultClear (&trans->result);

  if (offsetDown != 0) {
    SEEK (trans, offsetDown, SEEK_CUR);
  }

  trans->seekState.downLoc += offsetDown;
}

/*
 *------------------------------------------------------*
 *
 *	SeekPolicyGet --
 *
 *	Compute the currently used policy and store its
 *	name into the character buffer.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
SeekPolicyGet (trans, policy)
     TrfTransformationInstance* trans;
     char*                      policy;
{
  if (trans->seekCfg.identity) {
    /* identity forced */

    strcpy (policy, "identity");
    return;
  }

  if (!trans->seekState.allowed &&
      ((trans->seekState.used.numBytesTransform !=
	trans->seekCfg.chosen.numBytesTransform) ||
       (trans->seekState.used.numBytesDown !=
	trans->seekCfg.chosen.numBytesDown))) {
    /* unseekable forced */

    strcpy (policy, "unseekable");
    return;
  }

  /* chosen policy in effect */

  strcpy (policy, "");
  return;
}

/*
 *------------------------------------------------------*
 *
 *	SeekConfigGet --
 *
 *	Generates a list containing the current configuration
 *	of the seek system in a readable manner.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		An Tcl_Obj, or NULL.
 *
 *------------------------------------------------------*
 */

static Tcl_Obj*
SeekConfigGet (interp, cfg)
     Tcl_Interp* interp;
     SeekConfig* cfg;
{
  int      res;
  Tcl_Obj* list = (Tcl_Obj*) NULL;
  Tcl_Obj* sub1 = (Tcl_Obj*) NULL;
  Tcl_Obj* sub2 = (Tcl_Obj*) NULL;

  list = Tcl_NewListObj (0, NULL);

  if (list == (Tcl_Obj*) NULL) {
    goto error;
  }

  LIST_ADDSTR (error, list, "ratioNatural");
  sub1 = Tcl_NewListObj (0, NULL);

  if (sub1 == (Tcl_Obj*) NULL) {
    goto error;
  }

  LIST_ADDINT (error, sub1, cfg->natural.numBytesTransform);
  LIST_ADDINT (error, sub1, cfg->natural.numBytesDown);
  LIST_ADDOBJ (error, list, sub1);


  LIST_ADDSTR (error, list, "ratioChosen");
  sub2 = Tcl_NewListObj (0, NULL);

  if (sub2 == (Tcl_Obj*) NULL) {
    goto error;
  }

  LIST_ADDINT (error, sub2, cfg->chosen.numBytesTransform);
  LIST_ADDINT (error, sub2, cfg->chosen.numBytesDown);
  LIST_ADDOBJ (error, list, sub2);

  LIST_ADDSTR (error, list, "overideAllowed");
  LIST_ADDINT (error, list, cfg->overideAllowed);

  LIST_ADDSTR (error, list, "identityForced");
  LIST_ADDINT (error, list, cfg->identity);

  return list;

error:
  /* Cleanup any remnants of errors above */

  if (list != (Tcl_Obj*) NULL) {
    Tcl_DecrRefCount (list);
  }

  if (sub1 != (Tcl_Obj*) NULL) {
    Tcl_DecrRefCount (sub1);
  }

  if (sub2 != (Tcl_Obj*) NULL) {
    Tcl_DecrRefCount (sub2);
  }

  return NULL;
}

/*
 *------------------------------------------------------*
 *
 *	SeekStateGet --
 *
 *	Generates a list containing the current state of
 *	the seek system in a readable manner.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		An Tcl_Obj, or NULL.
 *
 *------------------------------------------------------*
 */

static Tcl_Obj*
SeekStateGet (interp, state)
     Tcl_Interp* interp;
     SeekState* state;
{
  int      res;
  Tcl_Obj* list = (Tcl_Obj*) NULL;
  Tcl_Obj* sub  = (Tcl_Obj*) NULL;

  list = Tcl_NewListObj (0, NULL);

  if (list == (Tcl_Obj*) NULL) {
    goto error;
  }

  LIST_ADDSTR (error, list, "seekable");
  LIST_ADDINT (error, list, state->allowed);

  LIST_ADDSTR (error, list, "ratio");

  sub  = Tcl_NewListObj (0, NULL);
  if (sub == (Tcl_Obj*) NULL) {
    goto error;
  }

  LIST_ADDINT (error, sub, state->used.numBytesTransform);
  LIST_ADDINT (error, sub, state->used.numBytesDown);
  LIST_ADDOBJ (error, list, sub);

  LIST_ADDSTR (error, list, "up");
  LIST_ADDINT (error, list, state->upLoc);

  LIST_ADDSTR (error, list, "upBufStart");
  LIST_ADDINT (error, list, state->upBufStartLoc);

  LIST_ADDSTR (error, list, "upBufEnd");
  LIST_ADDINT (error, list, state->upBufEndLoc);

  LIST_ADDSTR (error, list, "down");
  LIST_ADDINT (error, list, state->downLoc);

  LIST_ADDSTR (error, list, "downBase");
  LIST_ADDINT (error, list, state->downZero);

  LIST_ADDSTR (error, list, "downAhead");
  LIST_ADDINT (error, list, state->aheadOffset);

  LIST_ADDSTR (error, list, "changed");
  LIST_ADDINT (error, list, state->changed);

  return list;

error:
  /* Cleanup any remnants of errors above */

  if (list != (Tcl_Obj*) NULL) {
    Tcl_DecrRefCount (list);
  }

  if (sub != (Tcl_Obj*) NULL) {
    Tcl_DecrRefCount (sub);
  }

  return NULL;
}

#ifdef TRF_DEBUG
/*
 *------------------------------------------------------*
 *
 *	PrintString --
 *
 *	Defined only in debug mode, enforces correct
 *	printing of strings by adding a \0 after its value.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

void
PrintString (fmt,len,bytes)
     char* fmt;
     int   len;
     char* bytes;
{
  char* tmp = (char*) Tcl_Alloc (len+1);
  memcpy (tmp, bytes, len);
  tmp [len] = '\0';

  PRINT (fmt, len, tmp);

  Tcl_Free (tmp);
}

/*
 *------------------------------------------------------*
 *
 *	DumpString --
 *
 *	Defined only in debug mode, dumps information
 *	in hex blocks
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

void
DumpString (n,len,bytes)
     int   n;
     int   len;
     char* bytes;
{
  int i, c;

  for (i=0, c=0; i < len; i++, c++) {
    if (c == 0) {
      BLNKS;
    }

    printf (" %02x", (0xff & bytes [i]));

    if (c == 16) {
      c = -1;
      printf ("\n");
    }
  }

  if (c != 0) {
    printf ("\n");
  }
}

/*
 *------------------------------------------------------*
 *
 *	SeekDump --
 *
 *	Defined only in debug mode, dumps the complete
 *	state of all seek variables.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
SeekDump (trans, place)
     TrfTransformationInstance* trans;
     CONST char*                place;
{
  int         loc;
  Tcl_Channel parent = DOWNC (trans);

  loc = TELL (trans);

#if 0
  PRINT ("SeekDump (%s) {\n", place); FL; IN;

  PRINT ("ratio up:down    %d : %d\n",
	 trans->seekState.used.numBytesTransform,
	 trans->seekState.used.numBytesDown); FL;
  PRINT ("seekable         %d\n",
	 trans->seekState.allowed); FL;
  PRINT ("up               %d [%d .. %d]\n",
	 trans->seekState.upLoc,
	 trans->seekState.upBufStartLoc,
	 trans->seekState.upBufEndLoc); FL;
  PRINT ("down             %d [%d] | %d\n",
	 trans->seekState.downLoc,
	 trans->seekState.aheadOffset,
	 loc); FL;
  PRINT ("base             %d\n",
	 trans->seekState.downZero); FL;
  PRINT ("identity force   %d\n",
	 trans->seekCfg.identity); FL;
  PRINT ("seek while ident %d\n",
	 trans->seekState.changed); FL;
  PRINT ("read buffer      %d\n",
	 ResultLength (&trans->result)); FL;

  OT ; PRINT ("}\n"); FL;
#else
  PRINT ("SkDmp (%s) ", place); FL;

#if 0
  NPRINT ("(%2d : %2d) | ",
	  trans->seekCfg.natural.numBytesTransform,
	  trans->seekCfg.natural.numBytesDown); FL;
  NPRINT ("(%2d : %2d) | ",
	  trans->seekCfg.chosen.numBytesTransform,
	  trans->seekCfg.chosen.numBytesDown); FL;
#endif
  NPRINT ("%2d:%2d /%1d |r %5d |u %5d [%5d..%5d] |d %5d [%2d] %5d | %5d | %1d %1d",
	  trans->seekState.used.numBytesTransform,
	  trans->seekState.used.numBytesDown,
	  trans->seekState.allowed,
	  ResultLength (&trans->result),
	  trans->seekState.upLoc,
	  trans->seekState.upBufStartLoc,
	  trans->seekState.upBufEndLoc,
	  trans->seekState.downLoc,
	  trans->seekState.aheadOffset,
	  loc,
	  trans->seekState.downZero,
	  trans->seekCfg.identity,
	  trans->seekState.changed
	  ); FL;

  NPRINT ("\n"); FL;
#endif
}
#endif

/*
 *------------------------------------------------------*
 *
 *	AllocChannelType --
 *
 *	Allocates a new ChannelType structure.
 *	
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		A reference to the new structure.
 *
 *------------------------------------------------------*
 */

static Tcl_ChannelType*
AllocChannelType (sizePtr)
     int* sizePtr;
{
  /*
   * Allocation of a new channeltype structure is not easy, because of
   * the various verson of the core and subsequent changes to the
   * structure. The main challenge is to allocate enough memory for
   * modern versions even if this extension is compiled against one
   * of the older variants!
   *
   * (1) Versions before stubs (8.0.x) are simple, because they are
   *     supported only if the extension is compiled against exactly
   *     that version of the core.
   *
   * (2) With stubs we just determine the difference between the older
   *     and modern variant and overallocate accordingly if compiled
   *     against an older variant.
   */

  int size = sizeof(Tcl_ChannelType); /* Base size */

#ifdef USE_TCL_STUBS
  /*
   * Size of a procedure pointer. We assume that all procedure
   * pointers are of the same size, regardless of exact type
   * (arguments and return values).
   *
   * 8.1.   First version containing close2proc. Baseline.
   * 8.3.2  Three additional vectors. Moved blockMode, new flush- and
   *        handlerProc's.
   * 8.4+   wide seek, and thread action.
   *
   * => Compilation against earlier version has to overallocate five
   *    procedure pointers.
   */

#if !(GT832)
  size += 5 * procPtrSize;
#endif
#endif

  if (sizePtr != (int*) NULL) {
    *sizePtr = size;
  }
  return (Tcl_ChannelType*) Tcl_Alloc (size);
}

/*
 *------------------------------------------------------*
 *
 *	InitializeChannelType --
 *
 *	Initializes a new ChannelType structure.
 *	
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static Tcl_ChannelType*
InitializeChannelType (name, patchVariant)
     CONST char*      name;
     int              patchVariant;
{
  Tcl_ChannelType* tct;
  int              size;

  /*
   * Initialization of a new channeltype structure is not easy,
   * because of the various verson of the core and subsequent changes
   * to the structure. The main problem is if compiled against an
   * older version how to access the elements of the structure not
   * known in that version. It is made a bit easier because the
   * allocation routine returns the allocated size. This allows us to
   * clear out the entire structure. So we just have to deal with the
   * elements to set and not the ones left alone.
   */

  tct           = AllocChannelType (&size);
  tct->typeName = (char*) name;

  memset ((VOID*) tct, '\0', size);

  /*
   * Common elements of the structure (no changes in location or name)
   */

  tct->closeProc        = TrfClose;
  tct->inputProc        = TrfInput;
  tct->outputProc       = TrfOutput;
  tct->seekProc         = TrfSeek;
  tct->setOptionProc    = TrfSetOption;
  tct->getOptionProc    = TrfGetOption;
  tct->watchProc        = TrfWatch;
  tct->getHandleProc    = TrfGetFile;

  /*
   * No need to handle close2Proc. Already cleared with the 'memset'
   * above.
   */

  /*
   * blockModeProc is a twister. For 8.0.x we can access it
   * immediately. For the higher versions we have to make some
   * runtime-choices, and their implementation depends on the version
   * we compile against.
   */

#ifndef USE_TCL_STUBS
  /* 8.0.x */
  tct->blockModeProc    = TrfBlock;
#else
#if GT832
  /* 8.3.2. and higher. Direct access to all elements possible. Use
   *'patchVariant' information to select the values to use.
   */

  if ((patchVariant == PATCH_ORIG) ||
      (patchVariant == PATCH_82)) {
    /* The 'version' element of 8.3.2 is in the the place of the
     * blockModeProc. For the original patch in 8.1.x and the firstly
     * included (8.2) we have to set our blockModeProc into this
     * place.
     */
    tct->version = (Tcl_ChannelTypeVersion) TrfBlock;
  } else /* patchVariant == PATCH_832 */ {
    /* For the 8.3.2 core we present ourselves as a version 2
     * driver. This means a speciial value in version (ex
     * blockModeProc), blockModeProc in a different place and of
     * course usage of the handlerProc.
     */

    tct->version       = TCL_CHANNEL_VERSION_2;
    tct->blockModeProc = TrfBlock;
    tct->handlerProc   = TrfNotify;
  }
#else
  /* Same as above, but as we are compiling against an older core we
   * have to create some definitions for the new elements as the compiler
   * does not know them by name.
   */

  if ((patchVariant == PATCH_ORIG) ||
      (patchVariant == PATCH_82)) {
    /* The 'version' element of 8.3.2 is in the the place of the
     * blockModeProc. For the original patch in 8.1.x and the firstly
     * included (8.2) we have to set our blockModeProc into this
     * place.
     */
    tct->blockModeProc = TrfBlock;
  } else /* patchVariant == PATCH_832 */ {
    /* For the 8.3.2 core we present ourselves as a version 2
     * driver. This means a special value in version (ex
     * blockModeProc), blockModeProc in a different place and of
     * course usage of the handlerProc.
     */

#define TRF_CHANNEL_VERSION_2	((TrfChannelTypeVersion) 0x2)

#define BMP (*((Tcl_DriverBlockModeProc**) (&(tct->close2Proc) + 1)))
#define HP  (*((TrfDriverHandlerProc**)    (&(tct->close2Proc) + 3)))

    typedef struct TrfChannelTypeVersion_* TrfChannelTypeVersion;
    typedef int	(TrfDriverHandlerProc) _ANSI_ARGS_((ClientData instanceData,
						    int interestMask));

    tct->blockModeProc = (Tcl_DriverBlockModeProc*) TRF_CHANNEL_VERSION_2;

    BMP = TrfBlock;
    HP  = TrfNotify;

#undef BMP
#undef HP
#undef TRF_CHANNEL_VERSION_2
  }
#endif
#endif

  return tct;
}

/*
 *------------------------------------------------------*
 *
 *	TimerKill --
 *
 *	Timer management. Removes the internal timer
 *	if it exists.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
TimerKill (trans)
     TrfTransformationInstance* trans;
{
  if (trans->timer != (Tcl_TimerToken) NULL) {
    /* Delete an existing flush-out timer,
     * prevent it from firing on removed channel.
     */

    Tcl_DeleteTimerHandler (trans->timer);
    trans->timer = (Tcl_TimerToken) NULL;

    PRINT ("Timer deleted ..."); FL;
  }
}

/*
 *------------------------------------------------------*
 *
 *	TimerSetup --
 *
 *	Timer management. Creates the internal timer
 *	if it does not exist.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
TimerSetup (trans)
     TrfTransformationInstance* trans;
{
  if (trans->timer == (Tcl_TimerToken) NULL) {
    trans->timer = Tcl_CreateTimerHandler (TRF_DELAY, ChannelHandlerTimer,
					   (ClientData) trans);
  }
}

/*
 *------------------------------------------------------*
 *
 *	ChannelHandlerKS --
 *
 *	Management of channel handlers. Deletes/Recreates
 *	as required by the specified mask.
 *
 *	Sideeffects:
 *		See above.
 *
 *	Result:
 *		None.
 *
 *------------------------------------------------------*
 */

static void
ChannelHandlerKS (trans, mask)
     TrfTransformationInstance* trans;
     int                        mask;
{
  /*
   * This procedure is called only for the original and the 8.2
   * patch. The new 8.2.3 patch does not use channel handlers but a
   * separate NotifyHandler in the driver.
   */

  Tcl_Channel parent = DOWNC (trans);

  if (trans->watchMask) {
    /*
     * Remove event handler to underlying channel, this could
     * be because we are closing for real, or being "unstacked".
     */

    Tcl_DeleteChannelHandler (parent, ChannelHandler,
			      (ClientData) trans);
  }

  trans->watchMask = mask;

  if (trans->watchMask) {
    /*
     * Setup active monitor for events on underlying Channel
     */

    Tcl_CreateChannelHandler (parent, trans->watchMask,
			      ChannelHandler, (ClientData) trans);
  }
}
