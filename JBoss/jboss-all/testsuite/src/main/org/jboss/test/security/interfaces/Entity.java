package org.jboss.test.security.interfaces;

import javax.ejb.*;
import java.rmi.*;

/**

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public interface Entity extends EJBObject
{
    /** A method used to test Principal propagation. It
        returns the name of the value obtained from
        getCallerPrincipal().
     */
    public String echo(String arg) throws RemoteException;
}
