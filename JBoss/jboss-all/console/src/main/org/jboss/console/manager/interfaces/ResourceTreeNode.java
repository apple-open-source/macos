/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.manager.interfaces;

/**
 * A specific tree node that list manageable resources as sub-nodes
 *
 * @see org.jboss.console.manager.interfaces.TreeNode
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>December 16 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface ResourceTreeNode extends TreeNode
{
   public static final int ALWAYS_VISIBLE = 0;
   //public static final int INVISIBLE_IF_LEAF = 1;
   public static final int INVISIBLE_IF_SUBNODE_EXISTS = 2;

   public ManageableResource getResource ();
   
   public int getVisibility ();
}
