// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: Store.java,v 1.2.4.4 2003/07/30 23:18:19 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

import javax.servlet.http.HttpServletRequest;

//----------------------------------------

// a store provides 3 APIs :

// It's own start/stop methods. These will e.g. start/stop the session GC thread

// State LifeCyle methods - The Store encapsulates the LifeCycle of the State

// Session ID management methods - The session ID is a responsibility attribute of the store...

// Stores manage State, and will have to notify the Session Manager
// when they believe that this has timed-out.

public interface
  Store
  extends Cloneable
{
  Manager getManager();
  void setManager(Manager manager);

  // Store LifeCycle
  void start() throws Exception;
  void stop();
  void destroy();	// corresponds to ctor

  // Store accessors
  void setScavengerPeriod(int secs);
  void setScavengerExtraTime(int secs);
  void setActualMaxInactiveInterval(int secs);
  int getActualMaxInactiveInterval();
  boolean isDistributed();

  // ID allocation
  String allocateId(HttpServletRequest request) throws Exception;
  void   deallocateId(String id) throws Exception;

  // State LifeCycle
  State newState(String id, int maxInactiveInterval) throws Exception;
  State loadState(String id) throws Exception;
  void  storeState(State state) throws Exception;
  void  removeState(State state) throws Exception;

  // Store misc
  void scavenge() throws Exception;
  void passivateSession(StateAdaptor sa);

  public Object clone();
}

