/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.security.interfaces;

import java.rmi.RemoteException;
import java.security.Principal;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.directory.Attributes;

/** The project admin interface.

@see ProjRepositoryAdmin

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public interface IProjRepositoryAdmin
{
    public void createProject(Name projectName) throws NamingException, RemoteException;
    public void closeProject(Name projectName) throws NamingException, RemoteException;
    public Principal createUser(String userID) throws RemoteException;
    public void createProjectUser(Name projectName, Principal userID) throws RemoteException;
}
