// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: LocalStore.java,v 1.1.4.4 2003/07/30 23:18:19 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

import java.util.HashMap;
import java.util.Map;
import javax.servlet.http.HttpServletRequest;
import org.jboss.logging.Logger;

//----------------------------------------

public class LocalStore
  implements Store
{
  protected static final Logger _log=Logger.getLogger(LocalStore.class);
  Map _sessions=new HashMap();

  protected Manager _manager;
  public Manager getManager(){return _manager;}
  public void setManager(Manager manager){_manager=manager;}

  // Store LifeCycle
  public void start() {}
  public void stop() {}
  public void destroy() {}

  // State LifeCycle
  public State
    newState(String id, int maxInactiveInterval)
  {
    return new LocalState(id, maxInactiveInterval, _actualMaxInactiveInterval);
  }

  public State
    loadState(String id)
  {
    synchronized (_sessions) {return (State)_sessions.get(id);}
  }

  public void
    storeState(State state)
  {
    try
    {
      synchronized (_sessions) {_sessions.put(state.getId(), state);}
    }
    catch (Exception e)
    {
      _log.warn("could not store session");
    }
  }

  public void
    removeState(State state)
  {
    try
    {
      synchronized (_sessions) {_sessions.remove(state.getId());}
    }
    catch (Exception e)
    {
      _log.error("could not remove session", e);
    }
  }

  public String
    allocateId(HttpServletRequest request)
  {
    return getManager().getIdGenerator().nextId(request);
  }

  public void
    deallocateId(String id)
  {
  }

  public boolean
    isDistributed()
  {
    return false;
  }


  public void
    passivateSession(StateAdaptor sa)
  {
    // we don't do that !
    sa.invalidate();
  }

  // there is no need to scavenge distributed state - as there is none.
  public void setScavengerPeriod(int period) {}
  public void setScavengerExtraTime(int time) {}
  public void scavenge() {}

  protected int _actualMaxInactiveInterval=0;
  public void setActualMaxInactiveInterval(int interval) {_actualMaxInactiveInterval=interval;}
  public int getActualMaxInactiveInterval() {return _actualMaxInactiveInterval;}

  public Object
    clone()
  {
    LocalStore ls=new LocalStore();
    ls.setActualMaxInactiveInterval(_actualMaxInactiveInterval);
    return ls;
  }
}
