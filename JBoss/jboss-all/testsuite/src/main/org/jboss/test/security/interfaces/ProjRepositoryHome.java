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

/** A stateful session bean for accessing a hypothetical project information
repository. The information repository is similary to a JNDI store in
that items are accessed via a Name and the information is represented as
Attributes.

It is used to test non-declarative security.

@see javax.naming.Name
@see javax.naming.directory.Attributes

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public interface ProjRepositoryHome extends EJBHome
{
    public ProjRepository create(Name projectName) throws CreateException, RemoteException;
}
