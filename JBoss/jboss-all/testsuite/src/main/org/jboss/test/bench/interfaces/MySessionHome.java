package org.jboss.test.bench.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface MySessionHome extends EJBHome {
	
	public MySession create() throws RemoteException, CreateException;

} 
