// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: BindingInterceptor.java,v 1.1.4.3 2003/07/26 11:49:41 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import java.rmi.RemoteException;
import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpSessionBindingEvent;
import javax.servlet.http.HttpSessionBindingListener;
import org.jboss.logging.Logger;

//----------------------------------------

/**
 * A <code>BindingInterceptor</code> is responsible for notifying
 * HttpSessionAttributeListeners when Attributes are added to, removed
 * from or replaced in a Session and HttpSessionBindingListeners when
 * attributes are bound into or unbound from a Session. <P/> This is
 * an expensive Interceptor to use since it requires that on rebinding
 * an attribute in the Session, the old value is returned in case it
 * is a Listener requiring notification of it's removal.If you are
 * using a distributed store, this requirement will result in a
 * serialisation and network traffic overhead which may not actually
 * be necessary. Unfortunately it would be expensive to decide at
 * runtime whether to add this interceptor since one of these
 * Listeners might be bound into the session at any time from any
 * node. This may be addressed in a future release.
 *
 * @author <a href="mailto:jules@mortbay.com">Jules Gosnell</a>
 * @version 1.0
 */
public class BindingInterceptor
  extends StateInterceptor
{
  protected static final Logger _log=Logger.getLogger(BindingInterceptor.class);
  // All Interceptors are expected to provide this constructor...

  // HttpSessionBindingListeners are held in the SessionManager -
  // because they are attached at the webapp, rather than session
  // instance level. So they are notified via the Manager.

  protected Object
    notifyValueUnbound(String name, Object value)
  {
    if (!(value instanceof HttpSessionBindingListener))
      return value;

    HttpSessionBindingEvent event=new HttpSessionBindingEvent(getSession(), name, value);
    ((HttpSessionBindingListener)value).valueUnbound(event);
    event=null;

    return value;
  }

  protected Object
    notifyValueBound(String name, Object value)
  {
    if (!(value instanceof HttpSessionBindingListener))
      return value;

    HttpSessionBindingEvent event=new HttpSessionBindingEvent(getSession(), name, value);
    ((HttpSessionBindingListener)value).valueBound(event);
    event=null;

    return value;
  }

  // if we knew whether there were any HttpSessionAttributeListeners
  // registered with our Manager and the DistributedState knew whether
  // the oldValue was an HttpSessionBindingListener then we could make
  // the decision in the e.g. EJB tier as to whether to return the
  // oldValue or not. Unfortunately, all attributes are preserialised
  // in the Web Tier, in case the EJB tier doesn't have their class,
  // so this interceptor would have to mark relevant attributes on
  // their way into the EJB tier, so it would know whether to return
  // them in a set/removeAttribute situation later... - How can this
  // be done efficiently ?

  /**
   * <code>setAttribute</code> is overriden in order to notify
   * HttpSessionAttribute and HttpSessionBindingListeners of relative
   * Attribute changes.
   *
   * @param name a <code>String</code> value
   * @param value an <code>Object</code> value
   * @return an <code>Object</code> value
   * @exception RemoteException if an error occurs
   */
  public Object
    setAttribute(String name, Object value, boolean returnValue)
    throws RemoteException
  {
    // assert(name!=null);
    // assert(value!=null);

    boolean needOldValue=true;

    // we want the old binding back if it was an HttpSessionBindingListener
    Object oldValue=super.setAttribute(name, value, true);

    // send binding notifications

    try
    {
      if (oldValue!=null)
	notifyValueUnbound(name, oldValue);

      notifyValueBound(name, value);

      // send attribute notifications
      if (oldValue!=null)
	getManager().notifyAttributeReplaced(getSession(), name, oldValue);
      else
	getManager().notifyAttributeAdded(getSession(), name, value);
    }
    catch (Throwable t)
    {
      _log.error("error in user owned Listener - notifications may be incomplete", t);
    }

    return oldValue;
  }

  /**
   * <code>removeAttribute</code> is overriden in order to notify
   * HttpSessionAttribute and HttpSessionBindingListeners of relative
   * Attribute changes.
   *
   * @param name a <code>String</code> value
   * @return an <code>Object</code> value
   * @exception RemoteException if an error occurs
   */
  public Object
    removeAttribute(String name, boolean returnValue)
    throws RemoteException
  {
    // we want the old binding back if it was an HttpSessionBindingListener
    Object oldValue=super.removeAttribute(name, true);

    if (oldValue!=null)
    {
      try
      {
	notifyValueUnbound(name, oldValue);
	getManager().notifyAttributeRemoved(getSession(), name, oldValue);
      }
      catch (Throwable t)
      {
	_log.error("error in user owned Listener - notifications may be incomplete", t);
      }
    }

    return oldValue;
  }

  //  public Object clone() { return null; } // Stateful
}
