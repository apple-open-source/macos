package org.jboss.test.xa.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;

public interface XATestHome extends EJBHome {
    public XATest create() throws CreateException, RemoteException;
}
