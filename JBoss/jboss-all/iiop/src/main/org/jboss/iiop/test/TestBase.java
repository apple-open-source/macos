/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.test;

import javax.ejb.EJBObject;
import java.rmi.RemoteException;

public interface TestBase extends java.rmi.Remote {
   public final int const1 = 123;
   
   /**
    * Accessor of type Object
    */
   public Object getObjectValue() throws RemoteException;
}
