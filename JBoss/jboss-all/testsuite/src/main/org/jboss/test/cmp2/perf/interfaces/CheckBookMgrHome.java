package org.jboss.test.cmp2.perf.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface CheckBookMgrHome extends EJBHome
{
   public CheckBookMgr create(String account, double balance)
      throws CreateException, RemoteException;
}
