package org.jboss.test.jca.interfaces;

import java.rmi.RemoteException;

import javax.ejb.EJBObject;

public interface UnshareableConnectionSession
   extends EJBObject
{
   void runTest() throws RemoteException;
}
