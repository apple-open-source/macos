package org.jboss.test.bench.interfaces;

import java.rmi.RemoteException;

import javax.ejb.CreateException;
import javax.ejb.FinderException;
import javax.ejb.EJBHome;

public interface SimpleEntityHome extends EJBHome {
   
   public SimpleEntity create(int pk) throws RemoteException, CreateException;
   
   public SimpleEntity findByPrimaryKey(Integer pk) throws RemoteException, FinderException;
}
