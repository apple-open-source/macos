
package org.jboss.test.invokers.interfaces;

import java.rmi.RemoteException;
import java.util.Collection;
import javax.ejb.*;

public interface SimpleBMPHome
extends EJBHome
{
   public SimpleBMP create(int _id, String _name) 
      throws CreateException, RemoteException;

   public SimpleBMP findByPrimaryKey (Integer _key) 
      throws FinderException, RemoteException;
      
}
