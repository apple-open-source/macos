// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: CMPStore.java,v 1.3.2.3 2003/07/26 11:49:41 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.CreateException;
import javax.ejb.RemoveException;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.rmi.PortableRemoteObject;
import org.jboss.logging.Logger;
import org.mortbay.j2ee.session.interfaces.CMPState;
import org.mortbay.j2ee.session.interfaces.CMPStateHome;
import org.mortbay.j2ee.session.interfaces.CMPStatePK;
//----------------------------------------

public class CMPStore
  extends AbstractStore
{
  InitialContext _jndiContext;
  CMPStateHome   _home;
  String         _name="jetty/CMPState"; // TODO - parameterise

  // Store LifeCycle
  public void
    start()
    throws Exception
  {
    // jndi lookups here
    _jndiContext=new InitialContext();
    Object o=_jndiContext.lookup(_name);
    _home=(CMPStateHome)PortableRemoteObject.narrow(o, CMPStateHome.class);
    _log.info("Support for CMP-based Distributed HttpSessions loaded successfully: "+_home);

    super.start();
  }

  // State LifeCycle

  public State
    newState(String id, int maxInactiveInterval)
    throws RemoteException, CreateException
  {
    if (_home==null)
      throw new IllegalStateException("invalid store");

    Object tmp=_home.create(getManager().getContextPath(), id, maxInactiveInterval, _actualMaxInactiveInterval);
    CMPState state=(CMPState)PortableRemoteObject.narrow(tmp, CMPState.class);
    return state;
  }

  public State
    loadState(String id)
  {
    if (_home==null)
      throw new IllegalStateException("invalid store");

    try
    {
      return (CMPState)PortableRemoteObject.narrow(_home.findByPrimaryKey(new CMPStatePK(getManager().getContextPath(), id)), CMPState.class);
    }
    catch (Throwable e)
    {
      if (_log.isDebugEnabled()) _log.debug("session "+id+" not found: "+e);
      return null;
    }
  }

  public void
    storeState(State state)
  {
    // it's already stored...
  }

  public void
    removeState(State state)
    throws RemoteException, RemoveException
  {
    if (_home==null)
      throw new IllegalStateException("invalid store");

    ((CMPState)state).remove();
  }

  public boolean
    isDistributed()
  {
    return true;
  }

  public void
    scavenge()
    throws RemoteException
  {
    // run a GC method EJB-side to remove all Sessions whose
    // maxInactiveInterval+extraTime has run out...

    // no events (unbind, sessionDestroyed etc) will be raised Servlet
    // side on any node, but that's OK, because we know that the
    // session does not 'belong' to any of them, or they would have
    // already GC-ed it....

    _log.info("distributed scavenging...");
    _home.scavenge(_scavengerExtraTime, _actualMaxInactiveInterval);
  }

  public void
    passivateSession(StateAdaptor sa)
  {
    // we are already passivated...
  }
}
