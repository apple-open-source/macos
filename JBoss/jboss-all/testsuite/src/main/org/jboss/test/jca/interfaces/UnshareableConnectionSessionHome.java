package org.jboss.test.jca.interfaces;

import java.rmi.RemoteException;

import javax.ejb.CreateException;
import javax.ejb.EJBHome;

public interface UnshareableConnectionSessionHome 
   extends EJBHome
{
	public UnshareableConnectionSession create()
		throws RemoteException, CreateException;
}
