package org.jboss.test.classloader.scoping.transaction.interfaces;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

public interface TestSession
   extends EJBObject
{
   public void runTest() throws RemoteException;
   public int invokeNext(String id) throws RemoteException;
}
