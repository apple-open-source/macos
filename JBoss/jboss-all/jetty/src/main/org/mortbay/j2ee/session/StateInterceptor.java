// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: StateInterceptor.java,v 1.1.4.2 2002/11/16 21:58:58 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import java.rmi.RemoteException;
import java.util.Enumeration;
import java.util.Map;
import javax.servlet.http.HttpSession;

//----------------------------------------
/**
 * Superlass for StateInterceptors - objects which
 * wrap-n-delegate/decorate a State instance. A stack of
 * StateInterceptors form a StateContainer.
 *
 * @author <a href="mailto:jules@mortbay.com">Jules Gosnell</a>
 * @version 1.0
 */
public class
  StateInterceptor
  implements State, Cloneable
{
  //   protected final ThreadLocal _state  =new ThreadLocal();
  //   protected State       getState   ()                   {return (State)_state.get();}
  //   protected void        setState(State state)           {_state.set(state);}
  //
  private final static ThreadLocal _manager=new ThreadLocal();
  protected Manager     getManager ()                   {return (Manager)_manager.get();}
  protected void        setManager(Manager manager)     {_manager.set(manager);}

  private final static ThreadLocal _session=new ThreadLocal();
  protected HttpSession getSession ()                   {return (HttpSession)_session.get();}
  protected void        setSession(HttpSession session) {_session.set(session);}

  // management of this attribute needs to move into the container...
  private State _state;
  protected State       getState   ()                   {return _state;}
  protected void        setState(State state)           {_state=state;}

  //   protected HttpSession _session;
  //   protected HttpSession getSession ()                   {return _session;}
  //   protected void        setSession(HttpSession session) {_session=session;}

  //----------------------------------------
  // 'StateInterceptor' API
  //----------------------------------------

  // lifecycle
  public    void start() {}
  public    void stop() {}

  // misc
  public    String toString() {return "<"+getClass()+"->"+getState()+">";}

  //----------------------------------------
  // wrapped-n-delegated-to 'State' API
  //----------------------------------------
  // invariant field accessors
  public    String      getId()                                                      throws RemoteException {return getState().getId();}
  public    int         getActualMaxInactiveInterval()                               throws RemoteException {return getState().getActualMaxInactiveInterval();}
  public    long        getCreationTime()                                            throws RemoteException {return getState().getCreationTime();}

  // variant field accessors
  public    Map         getAttributes()                                              throws RemoteException {return getState().getAttributes();}
  public    void        setAttributes(Map attributes)                                throws RemoteException {getState().setAttributes(attributes);}
  public    long        getLastAccessedTime()                                        throws RemoteException {return getState().getLastAccessedTime();}
  public    void        setLastAccessedTime(long time)                               throws RemoteException {getState().setLastAccessedTime(time);}
  public    int         getMaxInactiveInterval()                                     throws RemoteException {return getState().getMaxInactiveInterval();}
  public    void        setMaxInactiveInterval(int interval)                         throws RemoteException {getState().setMaxInactiveInterval(interval);}

  // compound fn-ality
  public    Object      getAttribute(String name)                                    throws RemoteException {return getState().getAttribute(name);}
  public    Object      setAttribute(String name, Object value, boolean returnValue) throws RemoteException {return getState().setAttribute(name, value, returnValue);}
  public    Object      removeAttribute(String name, boolean returnValue)            throws RemoteException {return getState().removeAttribute(name, returnValue);}
  public    Enumeration getAttributeNameEnumeration()                                throws RemoteException {return getState().getAttributeNameEnumeration();}
  public    String[]    getAttributeNameStringArray()                                throws RemoteException {return getState().getAttributeNameStringArray();}
  public    boolean     isValid()                                                    throws RemoteException {return getState().isValid();}

  public Object
    clone()
    {
      Object tmp=null;
      try
      {
	tmp=getClass().newInstance();
      }
      catch (Exception e)
      {
	//	_log.error("could not clone "+getClass().getName(),e); - TODO
      }

      return tmp;
    }
}

