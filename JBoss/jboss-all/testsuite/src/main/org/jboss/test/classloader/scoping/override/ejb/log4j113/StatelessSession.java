package org.jboss.test.classloader.scoping.override.ejb.log4j113;

import java.rmi.RemoteException;
import javax.ejb.EJBObject;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface StatelessSession extends EJBObject
{
   /** Validate the the session sees the 1.1.3 version of log4j classes */
   public Throwable checkVersion() throws RemoteException;

}
