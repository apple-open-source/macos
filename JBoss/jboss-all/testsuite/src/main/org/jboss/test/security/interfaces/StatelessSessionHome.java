package org.jboss.test.security.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface StatelessSessionHome extends EJBHome
{
    public StatelessSession create() throws RemoteException, CreateException;
}
