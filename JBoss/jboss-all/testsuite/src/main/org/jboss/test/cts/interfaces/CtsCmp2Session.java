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


/** Interface for tests of stateless sessions
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2 $
 */
public interface CtsCmp2Session extends EJBObject
{
   public void testV1() throws RemoteException;
   public void testV2() throws RemoteException;
}
