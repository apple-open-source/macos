package org.jboss.test.testbean.interfaces;


import javax.ejb.EJBObject;
import java.rmi.RemoteException;

public interface BMTStateful extends EJBObject 
{
	public String txExists() throws RemoteException;

 	public String txCommit() throws RemoteException;

 	public String txRollback() throws RemoteException;

 	public String txBegin() throws RemoteException;
	
	public String txEnd() throws RemoteException;
	
	public String dbCommit() throws RemoteException;
	
	public String dbRollback() throws RemoteException;
	
	public String getDbField() throws RemoteException;
	
	public void createTable() throws RemoteException;
			
	public void dropTable() throws RemoteException;

	
}
