/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.navtree;

import org.jboss.console.manager.interfaces.ResourceTreeNode;
import org.jboss.console.manager.interfaces.TreeAction;
import org.jboss.console.manager.interfaces.TreeInfo;
import org.jboss.console.manager.interfaces.TreeNode;
import org.jboss.console.manager.interfaces.TreeNodeMenuEntry;

/**
 * NodeWrapper implementation for nodes that are not root nodes
 *
 * @see org.jboss.console.navtree.NodeWrapper
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.2.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>20 decembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class StdNodeWrapper 
   implements NodeWrapper
{
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------

   TreeNode node = null;
   NodeWrapper[] sons = null;
   TreeNode[] realSons = null;
   TreeInfo master = null;
   String path = null;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public StdNodeWrapper (TreeNode node, TreeInfo master, String parentPath)
   {
      this.node = node;
      this.master = master;
      this.path = parentPath + "/" + this.node.getName ();

      TreeNode[] plugged = null;
      if (node instanceof ResourceTreeNode)
      {
         plugged = master.getTreesForResource( ((ResourceTreeNode)node).getResource() );
         
         // Now we check which kind of visibility we want to give
         //
         int visibility = ((ResourceTreeNode)node).getVisibility();
         if (visibility == ResourceTreeNode.INVISIBLE_IF_SUBNODE_EXISTS &&
             plugged != null && plugged.length > 0)
         {
            this.node = getMasterFromPluggins (plugged);
            plugged = removeMasterFromList (plugged, this.node);
         }
      }
            
      TreeNode[] res = this.node.getNodeManagableResources ();
      TreeNode[] sub = this.node.getSubNodes ();
      
      if (res == null) res = new TreeNode[0];
      if (sub == null) sub = new TreeNode[0];
      if (plugged == null) plugged = new TreeNode[0];

      realSons = new TreeNode[res.length + sub.length + plugged.length];
      sons = new NodeWrapper[res.length + sub.length + plugged.length];

      for (int i=0; i<res.length; i++) 
         realSons[i] = res[i];
      for (int i=0; i<sub.length; i++) 
         realSons[res.length+i] = sub[i];         
      for (int i=0; i<plugged.length; i++) 
         realSons[res.length+sub.length+i] = plugged[i];         
   }

   // Public --------------------------------------------------------
   
   // Z implementation ----------------------------------------------
   
   public Object getChild (int index)
   {
      if (index >= sons.length)
         return null;

      if (sons[index] == null)
         sons[index] = new StdNodeWrapper(realSons[index], this.master, this.path);

      return sons[index];
   }

   public int getChildCount ()
   {
      return this.realSons.length;
   }

   public int getIndexOfChild (Object child)
   {
      for (int i=0; i<this.sons.length; i++)
      {
         if (this.sons[i] == child)
            return i;
      }
      return -1;         
   }

   public boolean isLeaf ()
   {
      return this.sons.length == 0;
   }

   public String toString ()
   {
      return this.node.getName ();
   }

   public String getIconUrl ()
   {
      return this.node.getIcon ();
   }

   public TreeAction getAssociatedAction ()
   {
      return this.node.getAction ();
   }
   
   public String getDescription ()
   {
      return this.node.getDescription ();
   }
   
   public TreeNodeMenuEntry[] getMenuEntries ()
   {
      return this.node.getMenuEntries ();
   }
   
   public String getPath ()
   {
      return this.path;
   }
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   protected TreeNode getMasterFromPluggins (TreeNode[] plugged)
   {
      for (int i=0; i<plugged.length; i++)
      {
         if (plugged[i].isMasterNode())
            return plugged[i];
      }
      
      return plugged[0];
      
   }
   
   protected TreeNode[] removeMasterFromList (TreeNode[] all, TreeNode main)
   {
      TreeNode[] result = new TreeNode[all.length-1];
      
      int cursor = 0;
      for (int i=0; i<all.length; i++)
      {
         if (all[i] != main)
         {
            result[cursor] = all[i];
            cursor++;
         }
      }
      
      return result;
   }
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
   
    
