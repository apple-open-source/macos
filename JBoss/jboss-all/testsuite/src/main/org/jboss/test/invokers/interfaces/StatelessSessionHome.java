package org.jboss.test.invokers.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface StatelessSessionHome extends EJBHome
{
    public StatelessSession create() throws RemoteException, CreateException;
}
