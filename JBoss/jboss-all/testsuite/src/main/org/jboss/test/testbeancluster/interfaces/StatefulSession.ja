/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.testbeancluster.interfaces;

import java.rmi.RemoteException;
import java.rmi.dgc.VMID;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>6. octobre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface StatefulSession extends org.jboss.test.testbean.interfaces.StatefulSession
{
   public NodeAnswer getNodeState () throws RemoteException;
   public void setName (String name) throws RemoteException;
   public void setNameOnlyOnNode (String name, VMID node) throws RemoteException;
}
