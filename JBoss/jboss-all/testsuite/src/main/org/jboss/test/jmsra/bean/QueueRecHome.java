/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package  org.jboss.test.jmsra.bean;

import java.rmi.RemoteException;
import javax.ejb.EJBHome;
import javax.ejb.CreateException;

/**
 * Home interface for QueueRec bean.
 *
 *
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>
 * @version $Revision: 1.1 $
 */
public interface QueueRecHome extends EJBHome {
    QueueRec create() throws RemoteException, CreateException;
}
