package org.jboss.test.hello.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBHome;
import javax.ejb.FinderException;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface HelloLogHome extends EJBHome
{
   public HelloLog findByPrimaryKey(String key)
      throws RemoteException, FinderException;
}
