/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.security.interfaces;

import javax.ejb.CreateException;
import javax.ejb.EJBHome;
import javax.naming.Name;
import java.rmi.RemoteException;

/** The home interface for the ProjRepositoryAdmin stateless session bean.

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public interface ProjRepositoryAdminHome extends EJBHome
{
    public void create() throws RemoteException, CreateException;
}
