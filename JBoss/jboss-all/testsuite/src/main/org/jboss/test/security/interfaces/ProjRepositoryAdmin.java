/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.security.interfaces;

import javax.ejb.EJBObject;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.directory.Attributes;
import java.rmi.RemoteException;

/** A stateless session bean for administiring projects.

It is used to test non-declarative security.

@see javax.naming.Name
@see javax.naming.directory.Attributes

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public interface ProjRepositoryAdmin extends EJBObject, IProjRepositoryAdmin
{
    // All methods come from the IProjRepositoryAdmin interface
}
