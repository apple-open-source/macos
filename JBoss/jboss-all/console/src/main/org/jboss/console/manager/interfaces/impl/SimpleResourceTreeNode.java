package org.jboss.console.manager.interfaces.impl;

import org.jboss.console.manager.interfaces.ManageableResource;
import org.jboss.console.manager.interfaces.ResourceTreeNode;
import org.jboss.console.manager.interfaces.TreeAction;
import org.jboss.console.manager.interfaces.TreeNode;
import org.jboss.console.manager.interfaces.TreeNodeMenuEntry;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.2.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>31 déc. 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class SimpleResourceTreeNode
   extends SimpleTreeNode
   implements ResourceTreeNode
{

   protected ManageableResource resource = null;
   protected int visibility = ResourceTreeNode.ALWAYS_VISIBLE;

   public SimpleResourceTreeNode()
   {
      super();
   }

   public SimpleResourceTreeNode(
      String name,
      String description,
      String icon,
      TreeAction action,
      TreeNodeMenuEntry[] menuEntries,
      TreeNode[] subNodes,
      ResourceTreeNode[] nodeManagableResources)
   {
      super(
         name,
         description,
         icon,
         action,
         menuEntries,
         subNodes,
         nodeManagableResources);
   }

   public SimpleResourceTreeNode(
      String name,
      String description,
      String icon,
      TreeAction action,
      TreeNodeMenuEntry[] menuEntries,
      TreeNode[] subNodes,
      ResourceTreeNode[] nodeManagableResources,
      ManageableResource resource)
   {
      super(
         name,
         description,
         icon,
         action,
         menuEntries,
         subNodes,
         nodeManagableResources);
      
      this.resource = resource;
   }

   public ManageableResource getResource()
   {
      return this.resource;
   }


   public int getVisibility()
   {
      return visibility;
   }

   public ResourceTreeNode setVisibility(int visibility)
   {
      this.visibility = visibility;
      return this;
   }

}
