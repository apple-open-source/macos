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
import javax.ejb.CreateException;
import javax.ejb.EJBObject;
import javax.ejb.RemoveException;
import org.jboss.test.cts.keys.AccountPK;


/**
 * Interface StatefulSession
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.10.2.3 $
 */
public interface StatefulSession
   extends EJBObject
{
   public String getTestName()
      throws RemoteException;

   public String method1 (String msg)
      throws RemoteException;

   public void incCounter ()
      throws RemoteException;

   public void decCounter ()
      throws RemoteException;

   public int getCounter ()
      throws RemoteException;

   public void setCounter (int value)
      throws RemoteException;

   public BeanContextInfo getBeanContextInfo ()
      throws RemoteException;

   public void loopbackTest ()
      throws RemoteException;

   public void loopbackTest (EJBObject obj)
      throws RemoteException;

   /** Has ejbActivate been called */
   public boolean getWasActivated() throws RemoteException;
   /** Has ejbPassivate been called */
   public boolean getWasPassivated() throws RemoteException;

   public void createLocalEntity(AccountPK pk, String personsName)
      throws CreateException, RemoteException;

   public String readAndRemoveEntity()
      throws RemoveException, RemoteException;

   public void createSessionHandle()
      throws RemoteException;

   public String useSessionHandle(String arg)
      throws RemoteException;

   public void createStatefulSessionHandle(String testName)
      throws RemoteException;
   public void useStatefulSessionHandle()
      throws RemoteException;

   public void createSessionRef()
      throws RemoteException;
   public String useSessionRef()
      throws RemoteException;

   public void ping()
      throws RemoteException;
   public void sleep(long wait)
      throws RemoteException;
}

