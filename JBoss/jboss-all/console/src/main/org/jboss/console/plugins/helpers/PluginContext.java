/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.plugins.helpers;

import org.jboss.console.manager.interfaces.ManageableResource;
import org.jboss.console.manager.interfaces.ResourceTreeNode;
import org.jboss.console.manager.interfaces.TreeNode;
import org.jboss.console.manager.interfaces.TreeNodeMenuEntry;

import javax.management.MBeanServer;
import javax.management.ObjectInstance;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>24 dec 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public interface PluginContext
{
   public String localizeUrl (String source);
   
   public org.jboss.logging.Logger getLogger();
   
   public MBeanServer getLocalMBeanServer ();
   public ObjectInstance[] getMBeansForClass (String scope, String className); 


   public TreeNode createTreeNode (String name,
                                            String description,
                                            String iconUrl,
                                            String defaultUrl,
                                            TreeNodeMenuEntry[] menuEntries,
                                            TreeNode[] subNodes,
                                            ResourceTreeNode[] subResNodes) throws Exception;

   public ResourceTreeNode createResourceNode (String name,
                                            String description,
                                            String iconUrl,
                                            String defaultUrl,
                                            TreeNodeMenuEntry[] menuEntries,
                                            TreeNode[] subNodes,
                                            ResourceTreeNode[] subResNodes,
                                            String jmxObjectName,
                                            String jmxClassName) throws Exception;

   public ResourceTreeNode createResourceNode (String name,
                                            String description,
                                            String iconUrl,
                                            String defaultUrl,
                                            TreeNodeMenuEntry[] menuEntries,
                                            TreeNode[] subNodes,
                                            ResourceTreeNode[] subResNodes,
                                            ManageableResource resource) throws Exception;

   public TreeNodeMenuEntry[] createMenus (String[] content) throws Exception;
   
   public String encode (String source);
}
