// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: TypeCheckingInterceptor.java,v 1.1.4.3 2003/07/26 11:49:41 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import java.rmi.RemoteException;
import javax.servlet.http.HttpSession;
import org.jboss.logging.Logger;

//----------------------------------------
// every time an attribute is added to the underlying HttpSession this
// interceptor ensures that the attribute type/value is one of :

// java.io.Serializable
// javax.ejb.EJBObject
// javax.ejb.EJBHome
// javax.transaction.UserTransaction - TODO - how/why ?
// javax.naming.Context - java:comp/env - we allow any Context

// TODO - How do we serialize Contexts and UserTransactions

public
  class TypeCheckingInterceptor
  extends StateInterceptor
{
  protected static final Logger _log=Logger.getLogger(TypeCheckingInterceptor.class);

  public Object
    setAttribute(String name, Object value, boolean returnValue)
    throws IllegalArgumentException, RemoteException
  {
    // SRV.7.7.2

    Object tmp=value;

    if (tmp!=null)
    {
      // The container may choose to support storage of other
      // designated objects in the HttpSession, such as references
      // to Enterprise JavaBean components and transactions.

      if (tmp instanceof javax.ejb.EJBObject)
	tmp=new SerializableEJBObject((javax.ejb.EJBObject)tmp);
      else if (tmp instanceof javax.ejb.EJBHome)
	tmp=new SerializableEJBHome((javax.ejb.EJBHome)tmp);
      else if (tmp instanceof javax.naming.Context)
	tmp=new SerializableContext((javax.naming.Context)tmp);
      else if (tmp instanceof javax.transaction.UserTransaction)
	tmp=new SerializableUserTransaction((javax.transaction.UserTransaction)tmp);
    }

    // The container must accept objects that implement the
    // Serializable interface
    if (tmp instanceof java.io.Serializable)
    {
      try
      {
	return super.setAttribute(name, tmp, returnValue);
      }
      catch (RemoteException e)
      {
	_log.error("could not set attribute", e);
	return null;
      }
    }
    else
    {
      // The servlet container may throw an IllegalArgumentException
      // if an object is placed into the session that is not
      // Serializable or for which specific support has not been made
      // available.
      throw new IllegalArgumentException("distributed attribute value must be Serializable,EJBObject,EJBHome,UserTransaction or Context: "+tmp);
    }
  }

  public Object
    getAttribute(String name)
    throws IllegalArgumentException, RemoteException
  {
    Object tmp=super.getAttribute(name);

    if (tmp!=null)
    {
      if (tmp instanceof org.mortbay.j2ee.session.SerializableEJBObject)
	return ((SerializableEJBObject)tmp).toEJBObject();
      else if (tmp instanceof org.mortbay.j2ee.session.SerializableEJBHome)
	return ((SerializableEJBHome)tmp).toEJBHome();
      else if (tmp instanceof org.mortbay.j2ee.session.SerializableContext)
	return ((SerializableContext)tmp).toContext();
      else if (tmp instanceof org.mortbay.j2ee.session.SerializableUserTransaction)
	return ((SerializableUserTransaction)tmp).toUserTransaction();
    }

    return tmp;
  }

  //  public Object clone() { return this; } // Stateless
}
