// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: CMRStore.java,v 1.1.2.3 2003/07/26 11:49:41 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import java.rmi.RemoteException;
import java.util.Timer;
import java.util.TimerTask;
import javax.ejb.CreateException;
import javax.ejb.CreateException;
import javax.ejb.RemoveException;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.rmi.PortableRemoteObject;
import org.jboss.logging.Logger;
import org.mortbay.j2ee.session.interfaces.CMRState;
import org.mortbay.j2ee.session.interfaces.CMRStateHome;
import org.mortbay.j2ee.session.interfaces.CMRStatePK;
//----------------------------------------

public class CMRStore
  extends AbstractStore
{
  InitialContext _jndiContext;
  CMRStateHome   _home;
  String         _name="jetty/CMRState"; // TODO - parameterise

  // Store LifeCycle
  public void
    start()
    throws Exception
  {
    // jndi lookups here
    _jndiContext=new InitialContext();
    Object o=_jndiContext.lookup(_name);
    _home=(CMRStateHome)PortableRemoteObject.narrow(o, CMRStateHome.class);
    _log.info("Support for CMP-based Distributed HttpSessions loaded successfully: "+_home);

    super.start();
  }

  // State LifeCycle
  public State
    loadState(String id)
  {
    if (_home==null)
      throw new IllegalStateException("invalid store");

    try
    {
      return (CMRState)PortableRemoteObject.narrow(_home.findByPrimaryKey(new CMRStatePK(getManager().getContextPath(), id)), CMRState.class);
    }
    catch (Throwable e)
    {
      _log.warn("session "+id+" not found: "+e);
      return null;
    }
  }

  public State
    newState(String id, int maxInactiveInterval)
    throws RemoteException, CreateException
  {
    if (_home==null)
      throw new IllegalStateException("invalid store");

    Object tmp=_home.create(getManager().getContextPath(), id, maxInactiveInterval);
    CMRState state=(CMRState)PortableRemoteObject.narrow(tmp, CMRState.class);
    return state;
  }

  public void
    storeState(State state)
  {
    // TODO
  }

  public void
    removeState(State state)
    throws RemoteException, RemoveException
  {
    if (_home==null)
      throw new IllegalStateException("invalid store");

    ((CMRState)state).remove();
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

    _home.scavenge(_scavengerExtraTime, _actualMaxInactiveInterval);
  }

  public void
    passivateSession(StateAdaptor sa)
  {
    // we are already passivated...
  }
}
