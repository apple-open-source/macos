#define Tcl_Interp_numLevels(in)       ((Interp *)in)->numLevels
#define Tcl_Interp_framePtr(in)        ((Tcl_CallFrame *)((Interp *)in)->framePtr)
#define Tcl_Interp_varFramePtr(in)     ((Tcl_CallFrame *)((Interp *)in)->varFramePtr)
#define Tcl_Interp_globalNsPtr(in)     ((Tcl_Namespace *)((Interp *)in)->globalNsPtr)
#define Tcl_Interp_flags(in)           ((Interp *)in)->flags
#if DISPATCH_TRACE
#define Tcl_Interp_returnCode(in)      ((Interp *)in)->returnCode
#endif
#define Tcl_Interp_threadId(in)        ((Interp *)in)->threadId

#define Tcl_CallFrame_callerPtr(cf)    ((Tcl_CallFrame*)((CallFrame *)cf)->callerPtr)
#define Tcl_CallFrame_procPtr(cf)      ((CallFrame *)cf)->procPtr
#define Tcl_CallFrame_varTablePtr(cf)  ((CallFrame *)cf)->varTablePtr
#define Tcl_CallFrame_level(cf)        ((CallFrame *)cf)->level
#define Tcl_CallFrame_isProcCallFrame(cf) ((CallFrame *)cf)->isProcCallFrame
#define Tcl_CallFrame_compiledLocals(cf)  ((CallFrame *)cf)->compiledLocals
#define Tcl_CallFrame_callerVarPtr(cf) ((Tcl_CallFrame*)((CallFrame *)cf)->callerVarPtr)
#define Tcl_CallFrame_objc(cf)            ((CallFrame *)cf)->objc
#define Tcl_CallFrame_objv(cf)            ((CallFrame *)cf)->objv

#define Tcl_Namespace_cmdTable(nsPtr) &((Namespace *)nsPtr)->cmdTable
#define Tcl_Namespace_varTable(nsPtr) &((Namespace *)nsPtr)->varTable
#define Tcl_Namespace_childTable(nsPtr) &((Namespace *)nsPtr)->childTable
#define Tcl_Namespace_activationCount(nsPtr) ((Namespace *)nsPtr)->activationCount

#define Tcl_Command_refCount(cmd)      ((Command *)cmd)->refCount
#define Tcl_Command_cmdEpoch(cmd)      ((Command *)cmd)->cmdEpoch
/* the following items could be obtained from 
   Tcl_GetCommandInfoFromToken(cmd, infoPtr) */
#define Tcl_Command_nsPtr(cmd)         ((Tcl_Namespace*)(((Command *)cmd)->nsPtr))
#define Tcl_Command_objProc(cmd)       ((Command *)cmd)->objProc
#define Tcl_Command_objClientData(cmd) ((Command *)cmd)->objClientData
#define Tcl_Command_proc(cmd)          ((Command *)cmd)->proc
#define Tcl_Command_clientData(cmd)    ((Command *)cmd)->clientData
#define Tcl_Command_deleteProc(cmd)    ((Command *)cmd)->deleteProc


/*
 * Conversion from CmdPtr to Class / Object
 */

static XOTCLINLINE ClientData
XOTclGetCDFromCmdPtr(Tcl_Command cmd) {
  assert(cmd);
  if (Tcl_Command_objProc(cmd) == XOTclObjDispatch && !Tcl_Command_cmdEpoch(cmd))
    return Tcl_Command_objClientData(cmd);
  else
    return NULL;
}

static XOTCLINLINE XOTclClass*
XOTclGetClassFromCmdPtr(Tcl_Command cmd) {
  ClientData cd = XOTclGetCDFromCmdPtr(cmd);
  if (cd) 
    return XOTclObjectToClass(cd);
  else
    return 0;
}

static XOTCLINLINE XOTclObject*
XOTclGetObjectFromCmdPtr(Tcl_Command cmd) {
  return (XOTclObject*) XOTclGetCDFromCmdPtr(cmd);
}
