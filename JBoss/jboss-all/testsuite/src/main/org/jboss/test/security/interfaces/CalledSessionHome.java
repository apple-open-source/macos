package org.jboss.test.security.interfaces;

import javax.ejb.CreateException;
import javax.ejb.EJBHome;
import java.rmi.RemoteException;

/**
 @author Scott.Stark@jboss.org
 @author $Revision: 1.2 $
 */
public interface CalledSessionHome extends EJBHome
{
   public CalledSession create() throws RemoteException, CreateException;
}
