package org.jboss.test.cts.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface StrictlyPooledSessionHome extends EJBHome
{
   public StrictlyPooledSession create() throws RemoteException, CreateException;
}
