package org.jboss.test.bench.ejb;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;

public class SimpleEntityBean implements EntityBean {
	public Integer pk;
	public int field;
	
	public Integer ejbCreate(int pk) throws RemoteException, CreateException {
		this.pk = new Integer(pk);
		field = 0;
		return null;
	}
	
	public void ejbPostCreate(int pk) throws RemoteException, CreateException {}
	
	public int getField() throws RemoteException {
		return field;
	}
	
	public void setField(int field) throws RemoteException {
		this.field = field;
	}
	
	public void ejbStore() throws RemoteException {}

	public void ejbLoad() throws RemoteException {}
	
	public void ejbActivate() throws RemoteException {}
	
	public void ejbPassivate() throws RemoteException {}
	
	public void ejbRemove() throws RemoteException {}

	public void setEntityContext(EntityContext e) throws RemoteException {}

	public void unsetEntityContext() throws RemoteException {}
	

}

