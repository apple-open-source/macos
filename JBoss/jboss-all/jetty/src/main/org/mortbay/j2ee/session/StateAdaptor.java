// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: StateAdaptor.java,v 1.2.2.3 2003/07/26 11:49:41 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import java.rmi.RemoteException;
import java.util.Enumeration;
import javax.servlet.ServletContext;
import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpSessionContext;
import org.jboss.logging.Logger;

//----------------------------------------
// this class is responsible for presenting a State container to a
// servlet as a HttpSession - since this interface is not an ideal one
// to use for the container interceptors. It constrains all the inputs
// to this API as specified...

// Since this is the front end of the Container, maybe it should be so
// called ? or maybe I should just call it [Http]Session?

// we should cache our id locally...

public class StateAdaptor
  implements org.mortbay.jetty.servlet.SessionManager.Session
{
  protected static final Logger _log=Logger.getLogger(StateAdaptor.class);
  Manager        _manager;
  State          _state=null;
  boolean        _new=true;

  // we cache these for speed...
  final String _id;

  StateAdaptor(String id, Manager manager, int maxInactiveInterval, long lastAccessedTime)
  {
    _id=id;
    _manager=manager;
  }

  void
    setState(State state)
  {
    _state=state;
  }

  State
    getState()
  {
    return _state;
  }

  // hmmmm...
  // why does Greg call this?
  // could it be compressed into a subsequent call ?
  public boolean
    isValid()
  {
    if (_state==null)
      return false;

    StateInterceptor si=(StateInterceptor)_state;
     si.setManager(_manager);
     si.setSession(this);

    try
    {
      _state.getLastAccessedTime(); // this should cause an interceptor/the session to check
      return true;
    }
    catch (IllegalStateException ignore)
    {
      return false;
    }
    catch (Exception e)
    {
      _log.error("problem contacting HttpSession", e);
      return false;
    }
  }

  // HttpSession API

  public long
    getCreationTime()
    throws IllegalStateException
  {
    checkState();

    try
    {
      return _state.getCreationTime();
    }
    catch (RemoteException e)
    {
      _log.error("could not get CreationTime", e);
      throw new IllegalStateException("problem with distribution layer");
    }
  }

  public String
    getId()
    throws IllegalStateException
  {
    checkState();

    // locally cached and invariant
    return _id;
  }

  public long
    getLastAccessedTime()
    throws IllegalStateException
  {
    checkState();

    try
    {
      return _state.getLastAccessedTime();
    }
    catch (RemoteException e)
    {
      _log.error("could not get LastAccessedTime", e);
      throw new IllegalStateException("problem with distribution layer");
    }
  }

  // clarify with Tomcat, whether this is on a per Session or SessionManager basis...
  public void
    setMaxInactiveInterval(int interval)
  {
    checkState();

    try
    {
      _state.setMaxInactiveInterval(interval);
    }
    catch (RemoteException e)
    {
      _log.error("could not set MaxInactiveInterval", e);
    }
  }

  public int
    getMaxInactiveInterval()
  {
    checkState();

    try
    {
      return _state.getMaxInactiveInterval();
    }
    catch (RemoteException e)
    {
      // Can I throw an exception of some type here - instead of
      // returning rubbish ? - TODO
      _log.error("could not get MaxInactiveInterval", e);
      return 0;
    }
  }

  public Object
    getAttribute(String name)
    throws IllegalStateException
  {
    checkState();

    try
    {
      return _state.getAttribute(name);
    }
    catch (RemoteException e)
    {
      _log.error("could not get Attribute", e);
      throw new IllegalStateException("problem with distribution layer");
    }
  }

  public Object
    getValue(String name)
    throws IllegalStateException
  {
    checkState();

    try
    {
      return _state.getAttribute(name);
    }
    catch (RemoteException e)
    {
      _log.error("could not get Value", e);
      throw new IllegalStateException("problem with distribution layer");
    }
  }

  public Enumeration
    getAttributeNames()
    throws IllegalStateException
  {
    checkState();

    try
    {
      return _state.getAttributeNameEnumeration();
    }
    catch (RemoteException e)
    {
      _log.error("could not get AttributeNames", e);
      throw new IllegalStateException("problem with distribution layer");
    }
  }

  public String[]
    getValueNames()
    throws IllegalStateException
  {
    checkState();

    try
    {
      return _state.getAttributeNameStringArray();
    }
    catch (RemoteException e)
    {
      _log.error("could not get ValueNames", e);
      throw new IllegalStateException("problem with distribution layer");
    }
  }

  public void
    setAttribute(String name, Object value)
    throws IllegalStateException
  {
    checkState();

    try
    {
      if (value==null)
	_state.removeAttribute(name, false);
      else
      {
	if (name==null)
	  throw new IllegalArgumentException("invalid attribute name: "+name);

	_state.setAttribute(name, value, false);
      }
    }
    catch (RemoteException e)
    {
      _log.error("could not set Attribute", e);
      throw new IllegalStateException("problem with distribution layer");
    }
  }

  public void
    putValue(String name, Object value)
    throws IllegalStateException
  {
    checkState();

    if (name==null)
      throw new IllegalArgumentException("invalid attribute name: "+name);

    if (value==null)
      throw new IllegalArgumentException("invalid attribute value: "+value);

    try
    {
      _state.setAttribute(name, value, false);
    }
    catch (RemoteException e)
    {
      _log.error("could not put Value", e);
      throw new IllegalStateException("problem with distribution layer");
    }
  }

  public void
    removeAttribute(String name)
    throws IllegalStateException
  {
    checkState();

    try
    {
      _state.removeAttribute(name, false);
    }
    catch (RemoteException e)
    {
      _log.error("could not remove Attribute", e);
      throw new IllegalStateException("problem with distribution layer");
    }
  }

  public void
    removeValue(String name)
    throws IllegalStateException
  {
    checkState();

    try
    {
      _state.removeAttribute(name, false);
    }
    catch (RemoteException e)
    {
      _log.error("could not remove Value", e);
      throw new IllegalStateException("problem with distribution layer");
    }
  }

  public void
    invalidate()
    throws IllegalStateException
  {
    if (_log.isTraceEnabled()) _log.trace("user invalidated session: "+getId());
    _manager.destroySession(this);
  }

  /**
   *
   * Returns <code>true</code> if the client does not yet know about the
   * session or if the client chooses not to join the session.  For
   * example, if the server used only cookie-based sessions, and
   * the client had disabled the use of cookies, then a session would
   * be new on each request.
   *
   * @return 				<code>true</code> if the
   *					server has created a session,
   *					but the client has not yet joined
   *
   * @exception IllegalStateException	if this method is called on an
   *					already invalidated session
   *
   */

  public boolean
    isNew()
    throws IllegalStateException
  {
    return _new;
  }

  public ServletContext
    getServletContext()
  {
    return _manager.getServletContext();
  }

  public HttpSessionContext
    getSessionContext()
  {
    return _manager.getSessionContext();
  }

  // this one's for Greg...
  public void
    access()
  {
    long time=System.currentTimeMillis(); // we could get this from Manager - less accurate
    setLastAccessedTime(time);

    _new=false;			// synchronise - TODO
  }

  public void
    setLastAccessedTime(long time)
    throws IllegalStateException
  {
    checkState();

    try
    {
      _state.setLastAccessedTime(time);
    }
    catch (RemoteException e)
    {
      _log.error("could not set LastAccessedTime", e);
      throw new IllegalStateException("problem with distribution layer");
    }
  }

  protected void
    checkState()
    throws IllegalStateException
  {
    if (_state==null)
      throw new IllegalStateException("invalid session");

    // this is a hack to get new interceptor stack to work... - TODO
    StateInterceptor si=(StateInterceptor)_state;
     si.setManager(_manager);
     si.setSession(this);
  }

  public String
    toString()
  {
    return "<"+getClass()+"->"+_state+">";
  }

  // I'm still not convinced that this is the correct place for this
  // method- but I can;t think of a better way - maybe in the next
  // iteration...

//   MigrationInterceptor _mi=null;
//
//   public void
//     registerMigrationListener(MigrationInterceptor mi)
//   {
//     _mi=mi;
//   }
//
   public void
     migrate()
   {
     //     if (_mi!=null)
     //       _mi.migrate(); // yeugh - TODO
   }
}
