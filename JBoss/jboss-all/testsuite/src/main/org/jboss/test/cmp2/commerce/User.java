package org.jboss.test.cmp2.commerce;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

public interface User extends EJBObject 
{
	public String getUserId() throws RemoteException;

	public String getUserName() throws RemoteException;
	public void setUserName(String name) throws RemoteException;

	public String getEmail() throws RemoteException;
	public void setEmail(String email) throws RemoteException;

	public boolean getSendSpam() throws RemoteException;
	public void setSendSpam(boolean sendSpam) throws RemoteException;
}
