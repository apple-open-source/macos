package org.jboss.test.cts.interfaces;

import java.rmi.*;
import javax.ejb.*;


/** The home for the CallerSession
 *
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.1.2.1 $
 */
public interface CallerSessionHome
   extends EJBHome
{
   public CallerSession create ()
      throws RemoteException, CreateException;
}
