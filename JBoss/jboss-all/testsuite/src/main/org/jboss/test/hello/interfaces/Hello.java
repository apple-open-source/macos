/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.test.hello.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

/** A simple hello world stateless session bean home
 *      
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.3.4.3 $
 */
public interface Hello
   extends EJBObject
{
   public String hello(String name)
      throws RemoteException;

   public String loggedHello(String name)
      throws RemoteException;

   public String helloException(String name)
      throws HelloException, RemoteException;

   public Hello helloHello(Hello object)
      throws RemoteException;

   public String howdy(HelloData name)
      throws RemoteException;

   /** A version of the hello method that sleeps for the indicated
    * time to test response delays.
    *
    * @param name some string to say Hello to.
    * @param sleepTimeMS Milliseconds to sleep
    * @return "Hello " + name.
    * @throws RemoteException
    */
   public String sleepingHello(String name, long sleepTimeMS)
      throws RemoteException;

   /** Access a method which returns an instance that will not be
    * found in the client env to check how CNFE are handled at the
    * transport layer.
    *
    * @return An HelloBean$ServerData
    * @throws RemoteException
    */
   public Object getCNFEObject()
         throws RemoteException;

   public void throwException()
      throws RemoteException;

   public NotSerializable getNotSerializable()
      throws RemoteException;

   public void setNotSerializable(NotSerializable ignored)
      throws RemoteException;
}
