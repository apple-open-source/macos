package org.jboss.test.cmp2.commerce;

import java.rmi.RemoteException; 
import java.util.Collection; 
import javax.ejb.CreateException;
import javax.ejb.EJBHome; 
import javax.ejb.FinderException; 

public interface CustomerHome extends EJBHome {
	public Customer create() throws RemoteException, CreateException;

	public Customer findByPrimaryKey(Long id) 
         throws RemoteException, FinderException;

	public Collection findByName(String name) 
         throws RemoteException, FinderException;
	
	public Collection findAll() 
         throws RemoteException, FinderException;
}
