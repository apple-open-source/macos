
package org.jboss.test.invokers.interfaces;

import java.rmi.RemoteException;
import javax.ejb.*;

public interface SimpleBMP
extends EJBObject
{
   public void setName (String _name) throws RemoteException;
   public String getName () throws RemoteException;
}
