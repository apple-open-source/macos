/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.test.threading.interfaces;

import java.rmi.*;
import javax.ejb.*;

/**
*   <description> 
*
*   @see <related>
*   @author  <a href="mailto:marc@jboss.org">Marc Fleury</a>
*   @version $Revision: 1.1 $
*   
*   Revisions:
*
*   20010625 marc fleury: Initial version
*/

public interface EJBThreadsHome extends EJBHome
{

	public EJBThreads create(String name) throws RemoteException, CreateException;
	public EJBThreads findByPrimaryKey(String name) throws FinderException, RemoteException;
	

}
