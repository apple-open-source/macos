// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: AroundInterceptor.java,v 1.1.2.3 2003/07/26 11:49:41 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import java.rmi.RemoteException;
import java.util.Collections;
import java.util.Enumeration;
import java.util.Map;
import javax.servlet.http.HttpSession;
import org.jboss.logging.Logger;

//----------------------------------------


public abstract class AroundInterceptor
  extends StateInterceptor
{
  protected abstract void before();
  protected abstract void after();

  public long
    getCreationTime()
    throws RemoteException
  {
    long tmp=0;

    before();
    try
    {
      tmp=super.getCreationTime();
    }
    finally
    {
      after();
    }

    return tmp;
  }

  public String
    getId()
    throws RemoteException
  {
    String tmp=null;

    before();
    try
    {
      tmp=super.getId();
    }
    finally
    {
      after();
    }

    return tmp;
  }

  public void
    setLastAccessedTime(long time)
    throws RemoteException
  {
    before();
    try
    {
      super.setLastAccessedTime(time);
    }
    finally
    {
      after();
    }
  }

  public long
    getLastAccessedTime()
    throws RemoteException
  {
    long tmp=0;

    before();
    try
    {
      tmp=super.getLastAccessedTime();
    }
    finally
    {
      after();
    }

    return tmp;
  }

  public void
    setMaxInactiveInterval(int interval)
    throws RemoteException
  {
    before();
    try
    {
      super.setMaxInactiveInterval(interval);
    }
    finally
    {
      after();
    }
  }

  public int
    getMaxInactiveInterval()
    throws RemoteException
  {
    int tmp=0;

    before();
    try
    {
      tmp=super.getMaxInactiveInterval();
    }
    finally
    {
      after();
    }

    return tmp;
  }

  public Object
    getAttribute(String name)
    throws RemoteException
  {
    Object tmp=null;

    before();
    try
    {
      tmp=super.getAttribute(name);
    }
    finally
    {
      after();
    }

    return tmp;
  }

  public Enumeration
    getAttributeNameEnumeration()
    throws RemoteException
  {
    Enumeration tmp=null;

    before();
    try
    {
      tmp=super.getAttributeNameEnumeration();
    }
    finally
    {
      after();
    }

    return tmp;
  }

  public String[]
    getAttributeNameStringArray()
    throws RemoteException
  {
    String[] tmp=null;

    before();
    try
    {
      tmp=super.getAttributeNameStringArray();
    }
    finally
    {
      after();
    }

    return tmp;
  }

  public Object
    setAttribute(String name, Object value, boolean returnValue)
    throws RemoteException
  {
    Object tmp=null;

    before();
    try
    {
      tmp=super.setAttribute(name, value, returnValue);
    }
    finally
    {
      after();
    }

    return tmp;
  }

  public Object
    removeAttribute(String name, boolean returnValue)
    throws RemoteException
  {
    Object tmp=null;

    before();
    try
    {
      tmp=super.removeAttribute(name, returnValue);
    }
    finally
    {
      after();
    }

    return tmp;
  }

  public Map
    getAttributes()
    throws RemoteException
  {
    Map tmp=null;

    before();
    try
    {
      tmp=super.getAttributes();
    }
    finally
    {
      after();
    }

    return tmp;
  }

  public void
    setAttributes(Map attributes)
    throws RemoteException
  {
    before();
    try
    {
      super.setAttributes(attributes);
    }
    finally
    {
      after();
    }
  }
}
