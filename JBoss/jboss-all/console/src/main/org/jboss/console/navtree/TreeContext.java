/**
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.navtree;

import org.jboss.console.manager.interfaces.SimpleTreeNodeMenuEntry;
import org.jboss.console.manager.interfaces.TreeAction;

/**
 * Interface used to communicate between the container-agnostic AdminTreeBrowser
 * and specific containers such as applets.
 *
 * @see org.jboss.console.navtree.AdminTreeBrowser
 * @see org.jboss.console.navtree.AppletBrowser
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>23 dec 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface TreeContext
{
   public void doAdminTreeAction (TreeAction action);
   
   public void doPopupMenuAction (SimpleTreeNodeMenuEntry entry);
   
   public java.util.Properties getJndiProperties ();
   
   public String getServiceJmxName ();
   
   public String localizeUrl (String sourceUrl);
   
   public org.jboss.console.remote.SimpleRemoteMBeanInvoker getRemoteMBeanInvoker ();
   
}
