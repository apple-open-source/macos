package org.jboss.test.bmp.interfaces;


import java.rmi.RemoteException;

import javax.ejb.*;


public interface BMPHelperSession
extends EJBObject
{
   public boolean existsSimpleBeanTable () throws RemoteException;
   public void createSimpleBeanTable () throws RemoteException;
   public void dropSimpleBeanTable () throws RemoteException;
   public String doTest () throws RemoteException;
   public String doTestAfterRollback () throws RemoteException;
}
