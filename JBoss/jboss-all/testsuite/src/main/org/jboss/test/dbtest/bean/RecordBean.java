package org.jboss.test.dbtest.bean;

import java.rmi.*;
import javax.ejb.*;

public class RecordBean implements EntityBean {
	private EntityContext entityContext;
	public String name;
	public String address;
	
	
	public String ejbCreate(String name) throws RemoteException, CreateException {
		
		this.name = name;
		this.address = "";
		return null;
	}
	
	public String ejbFindByPrimaryKey(String name) throws RemoteException, FinderException {
		
		return name;
	}
	
	public void ejbPostCreate(String name) throws RemoteException, CreateException {
	}
	
	public void ejbActivate() throws RemoteException {
	}
	
	public void ejbLoad() throws RemoteException {
	}
	
	public void ejbPassivate() throws RemoteException {
	}
	
	public void ejbRemove() throws RemoteException, RemoveException {
	}
	
	public void ejbStore() throws RemoteException {
	}
	
	public void setAddress(String address) {
		this.address = address;
	}
	
	public String getAddress() {
		return address;
	}
	
	public String getName() {
		return name;
	}
	
	
	public void setEntityContext(EntityContext context) throws RemoteException {
		entityContext = context;
	}
	
	public void unsetEntityContext() throws RemoteException {
		entityContext = null;
	}

}
