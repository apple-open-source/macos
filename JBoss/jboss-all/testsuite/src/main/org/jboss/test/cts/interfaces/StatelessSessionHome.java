package org.jboss.test.cts.interfaces;

import java.rmi.*;
import javax.ejb.*;


/** Basic stateless session home
 *
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.3.10.1 $
 */
public interface StatelessSessionHome
   extends EJBHome
{
   public StatelessSession create ()
      throws RemoteException, CreateException;

   /* The following included will not deploy as per the
      EJB 1.1 spec: [6.8] "There can be no other create methods
      in the home interface.
   public StatelessSession create( String aString);
   */
}
