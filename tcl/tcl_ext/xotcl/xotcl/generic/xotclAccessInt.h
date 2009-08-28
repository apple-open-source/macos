#define Tcl_Interp_numLevels(interp)       ((Interp *)interp)->numLevels
#define Tcl_Interp_framePtr(interp)        ((Tcl_CallFrame *)((Interp *)interp)->framePtr)
#define Tcl_Interp_varFramePtr(interp)     (((Interp *)interp)->varFramePtr)
#define Tcl_Interp_globalNsPtr(interp)     ((Tcl_Namespace *)((Interp *)interp)->globalNsPtr)
#define Tcl_Interp_flags(interp)           ((Interp *)interp)->flags
#if DISPATCH_TRACE
#define Tcl_Interp_returnCode(interp)      ((Interp *)interp)->returnCode
#endif
#define Tcl_Interp_threadId(interp)        ((Interp *)interp)->threadId

#define Tcl_CallFrame_callerPtr(cf)       ((Tcl_CallFrame*)((CallFrame *)cf)->callerPtr)
#define Tcl_CallFrame_procPtr(cf)         ((CallFrame *)cf)->procPtr
#define Tcl_CallFrame_varTablePtr(cf)     ((CallFrame *)cf)->varTablePtr
#define Tcl_CallFrame_level(cf)           ((CallFrame *)cf)->level
#define Tcl_CallFrame_isProcCallFrame(cf) ((CallFrame *)cf)->isProcCallFrame
#define Tcl_CallFrame_compiledLocals(cf)  ((CallFrame *)cf)->compiledLocals
#define Tcl_CallFrame_callerVarPtr(cf)    ((Tcl_CallFrame*)((CallFrame *)cf)->callerVarPtr)
#define Tcl_CallFrame_objc(cf)            ((CallFrame *)cf)->objc
#define Tcl_CallFrame_objv(cf)            ((CallFrame *)cf)->objv

#define Tcl_Namespace_cmdTable(nsPtr)    &((Namespace *)nsPtr)->cmdTable
#define Tcl_Namespace_varTable(nsPtr)    &((Namespace *)nsPtr)->varTable
#define Tcl_Namespace_childTable(nsPtr)  &((Namespace *)nsPtr)->childTable
#define Tcl_Namespace_activationCount(nsPtr) ((Namespace *)nsPtr)->activationCount
#define Tcl_Namespace_deleteProc(nsPtr)  ((Namespace *)nsPtr)->deleteProc



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
