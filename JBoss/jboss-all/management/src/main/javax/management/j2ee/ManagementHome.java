/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee;

import java.rmi.RemoteException;

import javax.ejb.CreateException;
import javax.ejb.EJBHome;

/**
 * Home Interface of the Management EJB
 *
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.4.1 $
 */
public interface ManagementHome
   extends EJBHome
{
   /** Create a Management session bean.
    * @return
    * @throws CreateException
    * @throws RemoteException
    */
   public Management create()
      throws CreateException,
         RemoteException;
}
