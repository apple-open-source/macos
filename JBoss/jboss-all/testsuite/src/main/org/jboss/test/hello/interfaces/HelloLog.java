package org.jboss.test.hello.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public interface HelloLog extends EJBObject
{
   public String getHelloArg() throws RemoteException;

   public long getStartTime() throws RemoteException;

   public long getEndTime() throws RemoteException;

   public long getElapsedTime() throws RemoteException;
}
