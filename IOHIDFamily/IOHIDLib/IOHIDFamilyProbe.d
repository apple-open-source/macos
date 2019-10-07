/*
 Function type:
 0: handleReport
 1: getReport
 2: setReport
 */
provider iohidfamily {
    
    probe hid_trace(uint32_t functionType, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5);
};
