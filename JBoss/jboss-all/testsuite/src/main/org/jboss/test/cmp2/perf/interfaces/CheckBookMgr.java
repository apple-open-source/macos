package org.jboss.test.cmp2.perf.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface CheckBookMgr extends EJBObject
{
   public int getEntryCount() throws RemoteException;
   public double getBalance() throws RemoteException;
}
