package org.jboss.test.exception;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;
import javax.ejb.FinderException;

public interface ExceptionTester extends EJBObject
{
   public void applicationExceptionInTx() 
         throws ApplicationException, RemoteException;
   public void applicationErrorInTx() throws RemoteException;
   public void ejbExceptionInTx() throws RemoteException;
   public void runtimeExceptionInTx() throws RemoteException;
   public void remoteExceptionInTx() throws RemoteException;
   
   public void applicationExceptionNewTx() 
         throws ApplicationException, RemoteException;
   public void applicationErrorNewTx() throws RemoteException;
   public void ejbExceptionNewTx() throws RemoteException;
   public void runtimeExceptionNewTx() throws RemoteException;
   public void remoteExceptionNewTx() throws RemoteException;

   public void applicationExceptionNoTx() 
         throws ApplicationException, RemoteException;
   public void applicationErrorNoTx() throws RemoteException;
   public void ejbExceptionNoTx() throws RemoteException;
   public void runtimeExceptionNoTx() throws RemoteException;
   public void remoteExceptionNoTx() throws RemoteException;
}
