/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.manager.interfaces.impl;

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
 * <p><b>31 dec 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class SimpleTreeNode implements TreeNode
{
   
   protected String name = null;
   protected String description = null;
   protected String icon = null;
   protected TreeAction action = null;
   protected TreeNodeMenuEntry[] menuEntries = null;
   protected TreeNode[] subNodes = null;
   protected ResourceTreeNode[] nodeManagableResources = null;
   protected boolean isMaster = false;
   
   public SimpleTreeNode (){ super(); }
   
   public SimpleTreeNode (String name,
                           String description,
                           String icon,
                           TreeAction action,
                           TreeNodeMenuEntry[] menuEntries,
                           TreeNode[] subNodes,
                           ResourceTreeNode[] nodeManagableResources)
   {  
      this.name = name;
      this.description = description;
      this.icon = icon;
      this.action = action;
      this.menuEntries = menuEntries;
      this.subNodes = subNodes;
      this.nodeManagableResources = nodeManagableResources;      
   }
   

   public String getName()
   {
      return this.name;
   }

   public String getDescription()
   {
      return this.description;
   }

   public String getIcon()
   {
      return this.icon;
   }

   public TreeAction getAction()
   {
      return this.action;
   }

   public TreeNodeMenuEntry[] getMenuEntries()
   {
      return this.menuEntries;
   }

   public TreeNode[] getSubNodes()
   {
      return this.subNodes;
   }

   public ResourceTreeNode[] getNodeManagableResources()
   {
      return this.nodeManagableResources;
   }
   
   public boolean isMasterNode ()
   {
      return this.isMaster;
   }

   public TreeNode setMasterNode (boolean master)
   {
      this.isMaster = master;
      return this;
   }

}
