package org.jboss.test.cts.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;

/**
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.2 $
 */
public interface CtsCmp2SessionHome extends EJBHome
{

   public CtsCmp2Session create()
      throws RemoteException, CreateException;
}

