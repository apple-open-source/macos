package org.jboss.test.jrmp.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface StatelessSession extends EJBObject
{
    /** A method that returns its arg */
    public String echo(String arg) throws RemoteException;
    /** A method that returns its arg arg as an IString */
    public IString copy(String arg) throws RemoteException;
}
