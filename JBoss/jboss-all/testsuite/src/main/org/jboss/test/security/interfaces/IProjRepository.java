/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.security.interfaces;

import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.directory.Attributes;
import java.rmi.RemoteException;

/** The business interface for the ProjRepository bean.

@see ProjRepository

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
 * @stereotype business-interface
*/
public interface IProjRepository
{
    public void createFolder(Name folderPath) throws NamingException, RemoteException;
    public void deleteFolder(Name folderPath, boolean recursive) throws NamingException, RemoteException;
    public void createItem(Name itemPath, Attributes attributes) throws NamingException, RemoteException;
    public void updateItem(Name itemPath, Attributes attributes) throws NamingException, RemoteException;
    public void deleteItem(Name itemPath) throws NamingException, RemoteException;
    public Attributes getItem(Name itemPath) throws NamingException, RemoteException;
}
