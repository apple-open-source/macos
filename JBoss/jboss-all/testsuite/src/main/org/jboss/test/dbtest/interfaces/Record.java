package org.jboss.test.dbtest.interfaces;


import javax.ejb.*;
import java.rmi.*;



public interface Record extends EJBObject {
	
	public String getName() throws RemoteException;
	public String getAddress() throws RemoteException;
	public void setAddress(String address) throws RemoteException;

}
