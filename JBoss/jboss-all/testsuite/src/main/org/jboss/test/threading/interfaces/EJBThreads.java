/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.test.threading.interfaces;

import java.rmi.*;
import javax.ejb.*;

public interface EJBThreads extends EJBObject
{
 
   public void test() throws RemoteException;
   public void testBusinessException() throws RemoteException;
   public void testRuntimeException() throws RemoteException;
   public void testTimeOut() throws RemoteException; 
   public void testNonTransactional() throws RemoteException;
  
}
