package org.jboss.test.testbean.interfaces;


import javax.ejb.EJBObject;
import java.rmi.RemoteException;

public interface BMTStateless extends EJBObject 
{
	public String txExists() throws RemoteException;
		
	public String txCommit() throws RemoteException;
		
	public String txRollback() throws RemoteException;

	public String txBegin() throws RemoteException;

 
}
