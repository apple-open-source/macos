package org.jboss.test.naming.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

/**

@author  Scott.Stark@jboss.org
@version $Revision: 1.1.10.1 $
*/
public interface TestENC extends EJBObject
{
   /** Perform interations count lookups against the ENC
    * @return the time spent in the method
    * @throws RemoteException
    */ 
   public long stressENC(long iterations) throws RemoteException;
}
