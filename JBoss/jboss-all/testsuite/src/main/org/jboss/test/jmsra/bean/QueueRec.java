/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package  org.jboss.test.jmsra.bean;

import javax.ejb.EJBObject;
import java.rmi.RemoteException;

/**
 * Remote interface for QueueRec bean.
 *
 *
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>
 * @version $Revision: 1.1 $
 */

public interface QueueRec extends EJBObject {
   /**
    * Get a message sync with jms ra.
    */
    public int getMessage() throws RemoteException;
}
