package org.jboss.test.security.interfaces;

import javax.ejb.CreateException;
import javax.ejb.EJBHome;
import java.rmi.RemoteException;

/** 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.6.1 $
 */
public interface StatefulSessionHome extends EJBHome
{
    public StatefulSession create(String state)
      throws RemoteException, CreateException;
}
