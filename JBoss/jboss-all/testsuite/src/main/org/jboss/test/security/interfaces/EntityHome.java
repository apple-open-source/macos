package org.jboss.test.security.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface EntityHome extends EJBHome
{
    public Entity findByPrimaryKey(String key) throws RemoteException, FinderException;
}
