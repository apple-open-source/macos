package org.jboss.test.dbtest.interfaces;


import javax.ejb.EJBHome;
import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.FinderException;

public interface RecordHome extends EJBHome {
	
	public Record create(String name) throws RemoteException, CreateException;
	
	public Record findByPrimaryKey(String name) throws RemoteException, FinderException;

}
