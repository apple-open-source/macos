/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.test.cts.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;


/** A session bean that calls another bean in a seperate deployment
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public interface CallerSession
   extends EJBObject
{
   /** A call that looks up the cts2.jar CalleeSessionHome from JNDI each
    * time, creates an instance and calls simpleCall(false) to test type
    * isolation between jars.
    * @param isCaller
    * @throws RemoteException
    */
   public CalleeData simpleCall(boolean isCaller) throws RemoteException;
   /** A call that looks up the cts2.jar CalleeSessionHome from JNDI once
    * and resuses the home on subsequent calls, creates an instance and calls
    * simpleCall(false) to test type isolation between jars.
    * @param isCaller
    * @throws RemoteException
    */
   public CalleeData simpleCall2(boolean isCaller) throws RemoteException;

   /** An entry point
    *
    * @throws RemoteException
    */
   public void callByValueInSameJar() throws RemoteException;
   /** Make a call that requires is test argument to be passed by value
    *
    * @param test argument used to test call marshalling
    * @throws RemoteException
    */
   public void validateValueMarshalling(ReferenceTest test)
      throws RemoteException;

   public void callAppEx()
      throws RemoteException, CalleeException;

   /** A method that throws an application exception
    */
   public void appEx()
      throws RemoteException, CalleeException;
}
