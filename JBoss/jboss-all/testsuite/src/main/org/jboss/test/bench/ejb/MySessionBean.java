package org.jboss.test.bench.ejb;

import java.rmi.*;
import javax.ejb.*;

public class MySessionBean implements SessionBean {
	private SessionContext sessionContext;
	
	public void ejbCreate() throws RemoteException, CreateException {
	}
	
	public void ejbActivate() throws RemoteException {
	}
	
	public void ejbPassivate() throws RemoteException {
	}
	
	public void ejbRemove() throws RemoteException {
	}
	
	public int getInt() {
		return 4;
	}
	
	public void setSessionContext(SessionContext context) throws RemoteException {
		sessionContext = context;
	}
}
