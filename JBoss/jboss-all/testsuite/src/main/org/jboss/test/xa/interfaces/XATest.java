package org.jboss.test.xa.interfaces;

import java.sql.SQLException;
import java.rmi.RemoteException;

import javax.ejb.EJBObject;
import javax.naming.NamingException;

public interface XATest
    extends EJBObject
{
    public void createTables()
        throws NamingException, SQLException, RemoteException;
    
    public void clearData() throws RemoteException;
    
    public void doWork() throws CantSeeDataException, RemoteException;
}
