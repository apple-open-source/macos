// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ActivationInterceptor.java,v 1.1.4.3 2003/07/26 11:49:41 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import java.rmi.RemoteException;
import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpSessionActivationListener;
import javax.servlet.http.HttpSessionEvent;
import org.jboss.logging.Logger;

//----------------------------------------

public class ActivationInterceptor
  extends StateInterceptor
{
  protected static final Logger _log=Logger.getLogger(ActivationInterceptor.class);
  protected final HttpSessionEvent _event;

  public
    ActivationInterceptor()
  {
    _event=new HttpSessionEvent(getSession()); // cache an event ready for use...
  }

  public Object
    getAttribute(String name)
    throws IllegalArgumentException, RemoteException
  {
    try
    {
      Object tmp=super.getAttribute(name);
      if (tmp!=null && tmp instanceof HttpSessionActivationListener)
 	((HttpSessionActivationListener)tmp).sessionDidActivate(_event);

      return tmp;
    }
    catch (Exception e)
    {
      _log.error("could not get Attribute: "+name, e);
      throw new IllegalArgumentException("could not get Attribute");
    }
  }

  public Object
    setAttribute(String name, Object value, boolean returnValue)
    throws IllegalArgumentException
  {
    try
    {
      Object tmp=value;
      if (tmp!=null && tmp instanceof HttpSessionActivationListener)
	((HttpSessionActivationListener)tmp).sessionWillPassivate(_event);

      tmp=super.setAttribute(name, tmp, returnValue);

      if (tmp!=null && tmp instanceof HttpSessionActivationListener)
	((HttpSessionActivationListener)tmp).sessionDidActivate(_event);

      return tmp;
    }
    catch (Exception e)
    {
      _log.error("could not set Attribute: "+name+":"+value, e);
      throw new IllegalArgumentException("could not set Attribute");
    }
  }

  // should an attribute be activated before it is removed ? How do we deal with the bind/unbind events... - TODO
  public Object
    removeAttribute(String name, boolean returnValue)
    throws IllegalArgumentException
  {
    try
    {
      Object tmp=super.removeAttribute(name, returnValue);

      if (tmp!=null && tmp instanceof HttpSessionActivationListener)
	((HttpSessionActivationListener)tmp).sessionDidActivate(_event);

      return tmp;
    }
    catch (Exception e)
    {
      _log.error("could not remove Attribute: "+name, e);
      throw new IllegalArgumentException("could not remove Attribute");
    }
  }

  //  public Object clone() { return null; } // Stateful
}
