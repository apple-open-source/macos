package org.jboss.test.cmp2.commerce;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;
import javax.ejb.FinderException;
import java.util.Collection;

public interface UserHome extends EJBHome
{
	public User create(String userId)
         throws RemoteException, CreateException;

	public User findByPrimaryKey(String id)
         throws RemoteException, FinderException;

	public User findByUserName(String userName)
         throws RemoteException, FinderException;

	public Collection findAll() throws RemoteException, FinderException;
	public Collection findAllByUserName(String userName) throws RemoteException, FinderException;
}

