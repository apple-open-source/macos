/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.manager.interfaces;

/**
 * Information about a specific node in the tree-browser
 *
 * @see org.jboss.console.manager.interfaces.TreeInfo
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>14 decembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface TreeNode
   extends java.io.Serializable
{
   public String getName ();
   public String getDescription ();
   
   public String getIcon ();
   
   public TreeAction getAction ();
   
   public TreeNodeMenuEntry[] getMenuEntries ();
   
   public TreeNode[] getSubNodes ();

   public ResourceTreeNode[] getNodeManagableResources ();
   
   /**
    * indicates if, in the presence of several nodes fighting for
    * the same ResourceTreeNode in mode INVISIBLE_IF_SUBNODE_EXISTS,
    * this node is master and other nodes (if any) will go as sub-nodes
    * of this one
    */
   public boolean isMasterNode ();
      
}
