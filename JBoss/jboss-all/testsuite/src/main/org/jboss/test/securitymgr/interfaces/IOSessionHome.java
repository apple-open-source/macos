package org.jboss.test.securitymgr.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface IOSessionHome extends EJBHome
{
    public IOSession create() throws RemoteException, CreateException;
}
