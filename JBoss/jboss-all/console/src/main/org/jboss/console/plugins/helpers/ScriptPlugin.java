/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.plugins.helpers;

import org.jboss.console.manager.interfaces.ManageableResource;
import org.jboss.console.manager.interfaces.TreeNode;

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
 * <p><b>24 dec 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public interface ScriptPlugin
{
   public String getVersion (PluginContext ctx);
   public String getName (PluginContext ctx);
   
   public boolean isResourceToBeManaged (ManageableResource resource,
                                           PluginContext ctx);
                                           
   public TreeNode getTreeForResource(ManageableResource resource,
                                          PluginContext ctx);

}
