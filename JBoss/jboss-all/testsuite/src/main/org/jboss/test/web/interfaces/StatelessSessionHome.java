package org.jboss.test.web.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface StatelessSessionHome extends EJBHome
{
    public StatelessSession create() throws RemoteException, CreateException;
}
