package org.jboss.test.testbean.interfaces;

import java.rmi.RemoteException;
import java.util.Collection;
import javax.ejb.EJBHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;


public interface EntityPKHome extends EJBHome {

    public EntityPK create(boolean aBoolean, int anInt, long aLong, double aDouble, String aString)
        throws RemoteException, CreateException;

    public EntityPK createMETHOD(boolean aBoolean, int anInt, long aLong, double aDouble, String aString)
        throws RemoteException, CreateException;

    public EntityPK findByPrimaryKey(AComplexPK acomplexPK)
        throws RemoteException, FinderException;
    public Collection findAll()
        throws RemoteException, FinderException;
}
