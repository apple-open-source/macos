static char
translate_state (state)
     int state;
{
  switch (state) {
  case TH_STATE_RUNNING:
    return ('R');
  case TH_STATE_STOPPED:
    return ('S');
  case TH_STATE_WAITING: 
    return ('W');
  case TH_STATE_UNINTERRUPTIBLE:
    return ('U');
  case TH_STATE_HALTED:		
    return ('H');
  default:
    return ('?');
  }
}

char *
getThreadNameFromState(USER_REG_STRUCT *userRegState, int n)
{
    static char buf[10];

#ifdef GDB414
    ur_cthread_t userReg;
    cthread_t incarnation;
    char *name;

    if ((userReg = WARP((ur_cthread_t)(USER_REG(*userRegState))))
        && (incarnation = WARP(userReg->incarnation))
	&& (name = WARPSTRING((char *)incarnation->name)))
 	return name;
    else {
	sprintf (buf, "_t%d", n);
	return buf;
    }
#else
    strcpy (buf, "bobo\n");
    return buf;
#endif
}

static char *
get_thread_name(thread_t th, int n)
{
    USER_REG_STRUCT threadState;
    unsigned int threadStateCount = USER_REG_COUNT;
    
    mach_check_ret(
      thread_get_state(th,
                       USER_REG_STATE,
                       (thread_state_t)&threadState,
		       &threadStateCount),
      "thread_get_state", "get_thread_name");
    return getThreadNameFromState(&threadState, n);
}

