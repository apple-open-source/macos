/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.iiop.test;

import javax.ejb.EJBObject;
import java.rmi.RemoteException;

/**
 */
public interface AbstractTestBase {
   /**
    * Accessor of type java.rmi.Remote
    */
   public java.rmi.Remote getSomethingRemote() throws RemoteException;
}
