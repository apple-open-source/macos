package org.jboss.test.security.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface StatelessSession extends EJBObject
{
    /** A method that returns its arg */
    public String echo(String arg) throws RemoteException;
    /** A method that does nothing but is not assiged a
     method-permission in the ejb-jar.xml descriptor
    */
    public void noop() throws RemoteException;
    /** A method that looks up the StatelessSession bean located at
     java:comp/env/ejb/Session and invokes echo(echoArg) on the
     bean and returns the result.
    */
    public String forward(String echoArg) throws RemoteException;
    /** A method that throws a NullPointerException */
    public void npeError() throws RemoteException;
    /** A method that is assigned the method-permission/unchecked tag
     to allow any authenticated user call the method.
     */
    public void unchecked() throws RemoteException;

    /** A method that is assigned to the exclude-list tag
     to indicate that no users should be allowed to call it.
     */
    public void excluded() throws RemoteException;
}
