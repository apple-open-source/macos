package org.jboss.test.classloader.scoping.override.ejb.log4j113;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface StatelessSessionHome extends EJBHome
{
   public StatelessSession create() throws RemoteException, CreateException;
}
