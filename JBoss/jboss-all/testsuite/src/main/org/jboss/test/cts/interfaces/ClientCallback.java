package org.jboss.test.cts.interfaces;

import java.rmi.Remote;
import java.rmi.RemoteException;

/** A callback interface passed in from the client
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2 $
 */
public interface ClientCallback extends Remote
{
   public String callback(String data) throws RemoteException;
}

