// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: PublishingInterceptor.java,v 1.1.2.4 2003/07/26 11:49:41 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------
import java.io.IOException;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.rmi.RemoteException;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import javax.servlet.http.HttpSession;
import org.jboss.logging.Logger;
//----------------------------------------

// at a later date, this needs to be able to batch up changes and
// flush to JG on various events e.g. immediately (no batching),
// end-of-request, idle, stop (migration)...

public class PublishingInterceptor
  extends StateInterceptor
{
  protected static final Logger _log=Logger.getLogger(PublishingInterceptor.class);

  protected AbstractReplicatedStore
    getStore()
  {
    AbstractReplicatedStore store=null;
    try
    {
      store=(AbstractReplicatedStore)getManager().getStore();
    }
    catch (Exception e)
    {
      _log.error("could not get AbstractReplicatedStore");
    }

    return store;
  }

  // by using a Proxy, we can avoid having to allocate/deallocate lots
  // of Class[]/Object[]s... Later it could be intelligent and not
  // pass the methodName and Class[] across the wire...
  public class PublishingInvocationHandler
    implements InvocationHandler
  {
    public Object
      invoke(Object proxy, Method method, Object[] args)
      throws RemoteException
    {
      if (!AbstractReplicatedStore.getReplicating())
	getStore().publish(getId(), method, args);

      return null;
    }
  }

  public void
    start()
  {
    _publisher=createProxy();
  }

  public State
    createProxy()
  {
    InvocationHandler handler = new PublishingInvocationHandler();
    return (State) Proxy.newProxyInstance(getStore().getLoader(), new Class[] { State.class }, handler);
  }

  protected State _publisher;

  //----------------------------------------
  // writers - wrap-publish-n-delegate - these should be moved into a
  // ReplicatingInterceptor...

  public void
    setLastAccessedTime(long time)
    throws RemoteException
  {
    _publisher.setLastAccessedTime(time);
    super.setLastAccessedTime(time);
  }

  public void
    setMaxInactiveInterval(int interval)
    throws RemoteException
  {
    _publisher.setMaxInactiveInterval(interval);
    super.setMaxInactiveInterval(interval);
  }

  public Object
    setAttribute(String name, Object value, boolean returnValue)
    throws RemoteException
  {
    _publisher.setAttribute(name, value, returnValue);
    return super.setAttribute(name, value, returnValue);
  }

  public void
    setAttributes(Map attributes)
    throws RemoteException
  {
    _publisher.setAttributes(attributes);
    super.setAttributes(attributes);
  }

  public Object
    removeAttribute(String name, boolean returnValue)
    throws RemoteException
  {
    _publisher.removeAttribute(name, returnValue);
    return super.removeAttribute(name, returnValue);
  }
}
