/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.manager.interfaces;

/**
 * Interface that merge all information about all plugins that is required
 * to build the tree structure used to browse managemeable resources
 *
 * @see org.jboss.console.manager.PluginManager
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>December 16, 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface TreeInfo 
   extends java.io.Serializable
{
   
   
   public ManageableResource[] getRootResources ();
   
   public void setRootResources (ManageableResource[] roots);
   
   public TreeNode[] getTreesForResource (ManageableResource resource);
   
   public void addTreeToResource (ManageableResource resource, TreeNode tree);
   
   public TreeAction getHomeAction ();
      
   public void setHomeAction (TreeAction homeAction);
   
   public String getDescription ();

   public String getIconUrl ();
   public void setIconUrl (String iconUrl);
   
   public void setRootMenus (TreeNodeMenuEntry[] menus);
   public TreeNodeMenuEntry[] getRootMenus ();
   
   public long getTreeVersion ();
   public void setTreeVersion (long version);
      
}
