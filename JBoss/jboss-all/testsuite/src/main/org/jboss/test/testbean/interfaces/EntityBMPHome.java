package org.jboss.test.testbean.interfaces;

import javax.ejb.EJBHome;
import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import java.util.Collection;
import java.util.Enumeration;

public interface EntityBMPHome extends EJBHome {
    
    public EntityBMP create()
        throws RemoteException, CreateException;

    public EntityBMP create(String name)
        throws RemoteException, CreateException;

    public EntityBMP createMETHOD(String name)
        throws RemoteException, CreateException;

    public EntityBMP findByPrimaryKey(String name)
        throws RemoteException, FinderException;
   
    public Collection findCollectionKeys(int num) 
            throws RemoteException, FinderException;
    
    public Enumeration findEnumeratedKeys(int num) 
            throws RemoteException, FinderException;

}
