package org.jboss.test.securitymgr.interfaces;

import javax.ejb.CreateException;
import javax.ejb.EJBHome;
import java.rmi.RemoteException;

public interface BadHome extends EJBHome
{
    public Bad create() throws RemoteException, CreateException;
}
