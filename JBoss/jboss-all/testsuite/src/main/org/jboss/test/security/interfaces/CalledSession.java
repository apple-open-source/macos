package org.jboss.test.security.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface CalledSession extends EJBObject
{
    /** A method that returns its arg */
    public String echo(String arg) throws RemoteException;
    /** A method that does nothing but is not assiged a
     method-permission in the ejb-jar.xml descriptor
    */
    public void noop() throws RemoteException;
}
