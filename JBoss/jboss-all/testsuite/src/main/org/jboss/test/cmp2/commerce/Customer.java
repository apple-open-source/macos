package org.jboss.test.cmp2.commerce;

import java.rmi.RemoteException; 
import javax.ejb.EJBObject; 

public interface Customer extends EJBObject {
	public Long getId() throws RemoteException;
	
	public String getName() throws RemoteException;
	
	public void setName(String name) throws RemoteException;
	
	public User getUser() throws RemoteException;
	
	public void setUser(User user) throws RemoteException;
}
