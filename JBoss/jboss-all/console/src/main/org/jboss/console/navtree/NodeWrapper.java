/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.navtree;

import org.jboss.console.manager.interfaces.TreeAction;
import org.jboss.console.manager.interfaces.TreeNodeMenuEntry;

/**
 * Base interface for wrapper objects used to represent tree nodes
 *
 * @see org.jboss.console.navtree.ConsoleTreeModel
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>20 decembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface NodeWrapper
{
   public Object getChild (int index);      
   public int getChildCount ();
   public int getIndexOfChild (Object child);
   public boolean isLeaf ();

   public String getIconUrl ();
   public TreeAction getAssociatedAction();
   public String getDescription ();
   public TreeNodeMenuEntry[] getMenuEntries ();
   
   public String getPath ();
}
