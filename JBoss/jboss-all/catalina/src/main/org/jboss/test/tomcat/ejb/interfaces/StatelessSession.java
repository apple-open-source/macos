package org.jboss.test.tomcat.ejb.interfaces;

import javax.ejb.*;
import java.rmi.*;


public interface StatelessSession extends EJBObject
{
    public String getMessage() throws RemoteException;
    public String getMessageAndTrace() throws RemoteException;
}
