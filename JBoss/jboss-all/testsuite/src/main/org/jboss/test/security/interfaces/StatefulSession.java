package org.jboss.test.security.interfaces;

import javax.ejb.EJBObject;
import java.rmi.RemoteException;

/** 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.6.1 $
 */
public interface StatefulSession extends EJBObject
{
    /** A method that returns its arg */
    public String echo(String arg) throws RemoteException;
}
