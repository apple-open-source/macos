package org.jboss.test.security.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface IOSessionHome extends EJBHome
{
    public IOSession create() throws RemoteException, CreateException;
}
