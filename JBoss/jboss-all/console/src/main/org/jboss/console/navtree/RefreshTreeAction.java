/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.navtree;

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
 * <p><b>2 janv. 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class RefreshTreeAction implements AppletTreeAction
{
   
   protected boolean forceRefresh = false;

   public RefreshTreeAction (boolean force)
   {
      this.forceRefresh = force;
   }

   public void doAction(TreeContext tc, AppletBrowser applet)
   {
      applet.refreshTree(this.forceRefresh);
   }

}
