//
//  KDState.hpp
//  KDBG
//
//  Created by James McIlree on 8/10/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

class KDState {
  protected:
    kbufinfo_t		_state;

  public:
    uint32_t flags()			{ return _state.flags; }
    int capacity()			{ return _state.nkdbufs; }
    int thread_map_capacity()		{ return _state.nkdthreads; }
    
    bool is_enabled()			{ return !_state.nolog; }
    bool is_initialized()		{ return flags() & KDBG_BUFINIT; }
    bool is_thread_map_initialized()	{ return flags() & KDBG_MAPINIT; }
    bool is_nowrap()			{ return flags() & KDBG_NOWRAP; }
    bool is_freerun()			{ return flags() & KDBG_FREERUN; }
    bool is_wrapped()			{ return flags() & KDBG_WRAPPED; }
    
    bool is_lp64()			{ return (flags() & KDBG_LP64) > 0; }

    bool is_range_collection_enabled()		{ return (flags() & KDBG_RANGECHECK) > 0; }
    bool is_specific_value_collection_enabled()	{ return (flags() & KDBG_VALCHECK) > 0; }
    bool is_filter_collection_enabled()		{ return (flags() & KDBG_TYPEFILTER_CHECK) > 0; }
    bool is_inclusive_pid_collection_enabled()	{ return (flags() & KDBG_PIDCHECK) > 0; }
    bool is_exclusive_pid_collection_enabled()	{ return (flags() & KDBG_PIDEXCLUDE) > 0; }

    pid_t controlling_pid()		{ return _state.bufid; }
    
    void print() {
        printf("KDebug State\n");
        printf("\tBuffer is %s\n", this->is_initialized() ? "initialized" : "not initialized");
        printf("\tCapacity is %d\n", this->capacity());
        printf("\tRaw flags 0x%08x\n", this->flags());
        printf("\tLogging is %s\n", this->is_enabled() ? "enabled" : "disabled");
        printf("\tWrapping is %s\n", this->is_nowrap() ? "disabled" : "enabled");
	printf("\tBuffer %s wrapped\n", this->is_wrapped() ? "has" : "has not");
	
        // Two bits, 4 possible states:
        //
        // INC    EXC
        // 1      0     ALL_MARKED_PIDS
        // 0      1     ALL_UNMARKED_PIDS
        // 0      0     ALL_PIDS
        // 1      1     ERROR

        const char* style;
        switch (flags() & (KDBG_PIDEXCLUDE | KDBG_PIDCHECK)) {
            case 0:
                style = "all-pids";
                break;
            case KDBG_PIDCHECK:
                style = "includes-marked-pids";
                break;
            case KDBG_PIDEXCLUDE:
                style = "excludes-marked-pids";
                break;
            default:
                style = "ERROR";
                break;
        }
        printf("\tCollection style is %s\n", style);
        printf("\tCollection by range is %s\n", this->is_range_collection_enabled() ? "enabled" : "disabled");
        printf("\tCollection by value is %s\n", this->is_specific_value_collection_enabled() ? "enabled" : "disabled");
        printf("\tCollection by filter is %s\n", this->is_filter_collection_enabled() ? "enabled" : "disabled");
        printf("\tThread map is %s ", this->is_thread_map_initialized() ? "initialized\n" : "not initialized\n");
        printf("\tThread map entries %d\n", this->thread_map_capacity());
        if (this->controlling_pid() == -1)
            printf("\tNo controlling pid\n");
        else
            printf("\tControlled by pid %d\n", this->controlling_pid());
    }
};

