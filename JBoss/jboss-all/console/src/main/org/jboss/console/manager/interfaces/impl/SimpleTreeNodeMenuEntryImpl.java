/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.manager.interfaces.impl;

import org.jboss.console.manager.interfaces.SimpleTreeNodeMenuEntry;
import org.jboss.console.manager.interfaces.TreeAction;

/**
 * Default implementation for a simple entry of a popup menu
 *
 * @see org.jboss.console.manager.interfaces.TreeNodeMenuEntry
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>17 decembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class SimpleTreeNodeMenuEntryImpl
   implements SimpleTreeNodeMenuEntry
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   protected TreeAction action = null;
   protected String text = null;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public SimpleTreeNodeMenuEntryImpl () {}
   
   public SimpleTreeNodeMenuEntryImpl (String text, TreeAction action)
   {
      this.action = action;
      this.text = text;
   }
   
   // Public --------------------------------------------------------
   
   // SimpleTreeNodeMenuEntry implementation ------------------------
   
   public TreeAction getAction ()
   {
      return this.action;
   }
   
   public String getText ()
   {
      return this.text;
   }
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
