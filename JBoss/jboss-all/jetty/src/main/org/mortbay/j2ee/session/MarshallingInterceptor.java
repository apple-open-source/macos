// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: MarshallingInterceptor.java,v 1.1.4.5 2003/07/26 11:49:41 jules_gosnell Exp $
// ========================================================================

package org.mortbay.j2ee.session;

//----------------------------------------

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectOutputStream;
import java.rmi.RemoteException;
import java.util.HashMap;
import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpSessionActivationListener;
import javax.servlet.http.HttpSessionEvent;
import org.jboss.logging.Logger;

//----------------------------------------

// If we distributed instances via tiers that knew nothing about their
// type (EJB/DB) they would not be able to handle them, so we send
// them all as Strings. This involves marshalling them on the way out
// and demarshalling them on the way back. The relevant
// activation/passivation notifications are made during this process.

// I should probably separate Marshalling from ActivationNotifying in
// case someone knows that they are only using classes that will be
// present in the e.g. ejb tier and can avoid the marshalling
// overhead.

public class MarshallingInterceptor
  extends StateInterceptor
{
  protected static final Logger _log=Logger.getLogger(MarshallingInterceptor.class);

  static class ObjectInputStream
    extends java.io.ObjectInputStream
  {
    ObjectInputStream(InputStream is)
      throws IOException
    {
      super(is);
    }

    private static final HashMap _primClasses = new HashMap(8, 1.0F);

    static
    {
      _primClasses.put("boolean" , boolean.class);
      _primClasses.put("byte"    , byte.class);
      _primClasses.put("char"    , char.class);
      _primClasses.put("short"   , short.class);
      _primClasses.put("int"     , int.class);
      _primClasses.put("long"    , long.class);
      _primClasses.put("float"   , float.class);
      _primClasses.put("double"  , double.class);
      _primClasses.put("void"    , void.class);
    }

    // is this really necessary ?
    protected Class
      resolveClass(java.io.ObjectStreamClass desc)
      throws IOException, ClassNotFoundException
    {
      String name = desc.getName();
      try
      {
	return Class.forName(name, false, Thread.currentThread().getContextClassLoader());
      }
      catch (ClassNotFoundException ex)
      {
	Class cl = (Class) _primClasses.get(name);
	if (cl != null)
	  return cl;
	else
	  throw ex;
      }
    }
  }

  static public byte[]
    marshal(Object value)
    throws IOException
  {
    if (value==null)
      return null;

    ByteArrayOutputStream baos=new ByteArrayOutputStream();
    ObjectOutputStream    oos =new ObjectOutputStream(baos);
    oos.writeObject(value);
    oos.flush();
    return baos.toByteArray();
  }

  static public Object
    demarshal(byte[] buffer)
    throws IOException,ClassNotFoundException
  {
    if (buffer==null)
      return buffer;

    ByteArrayInputStream bais=new ByteArrayInputStream(buffer);
    ObjectInputStream    ois =new ObjectInputStream(bais);
    return ois.readObject();
  }

  public Object
    getAttribute(String name)
    throws IllegalArgumentException, RemoteException
  {
    try
    {
      Object tmp=demarshal((byte[])super.getAttribute(name));
//       if (tmp!=null && tmp instanceof HttpSessionActivationListener)
// 	((HttpSessionActivationListener)tmp).sessionDidActivate(new HttpSessionEvent(_session));

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
      if (tmp!=null)
      {
// 	if (tmp instanceof HttpSessionActivationListener)
// 	  ((HttpSessionActivationListener)tmp).sessionWillPassivate(new HttpSessionEvent(_session));
 	tmp=marshal(tmp);
      }
      return demarshal((byte[])super.setAttribute(name, tmp, returnValue));
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
      // should this be activated - probably
      return demarshal((byte[])super.removeAttribute(name, returnValue));
    }
    catch (Exception e)
    {
      _log.error("could not remove Attribute: "+name, e);
      throw new IllegalArgumentException("could not remove Attribute");
    }
  }

  //  public Object clone() { return this; } // Stateless
}
