/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.manager.interfaces.impl;

import org.jboss.console.manager.interfaces.TreeAction;

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
 * <p><b>17. décembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class HttpLinkTreeAction
   implements TreeAction
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   protected String target = null;
   protected String frame = null;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public HttpLinkTreeAction () {}
   public HttpLinkTreeAction (String target)
   {
      this.target = target;
   }
   
   public HttpLinkTreeAction (String target, String frame)
   {
      this.target = target;
      this.frame = frame;
   }
   
   // Public --------------------------------------------------------
   
   public String getTarget ()
   {
      return this.target;
   }
   
   public String getFrame ()
   {
      return this.frame;
   }
   
   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
