/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.hellojrmpiiop.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

/** A simple hello world stateless session bean home
 *      
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.1 $
 */
public interface Hello
   extends EJBObject
{
   public String hello(String name)
      throws RemoteException;

   public Hello helloHello(Hello object)
      throws RemoteException;

   public String howdy(HelloData name)
      throws RemoteException;

   public void throwException()
      throws RemoteException;
}
