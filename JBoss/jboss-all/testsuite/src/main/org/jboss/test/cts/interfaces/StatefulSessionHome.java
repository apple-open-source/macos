/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cts.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;

/** The remote home interface for stateful session tests
 * 
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.7.2.1 $
 */
public interface StatefulSessionHome
   extends EJBHome
{
   public StatefulSession create(String testName)
      throws RemoteException, CreateException;
   /** A test of the alternate ejbCreate<METHOD> form of create
    * @param testName
    * @return The StatefulSession remote proxy
    * @throws RemoteException thrown on transport error
    * @throws CreateException thrown on container error
    */ 
   public StatefulSession createAlt(String testName)
      throws RemoteException, CreateException;
}
