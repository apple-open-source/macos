/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.testbeancluster.interfaces;

import java.rmi.dgc.VMID;

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
 * <p><b>6. octobre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class NodeAnswer implements java.io.Serializable
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   public VMID nodeId = null;
   public Object answer = null;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public NodeAnswer (VMID node, Object answer)
   {
      this.nodeId = node;
      this.answer = answer;
   }
   
   // Public --------------------------------------------------------
   
   public VMID getNodeId ()
   {
      return this.nodeId;
   }
   
   public Object getAnswer()
   {
      return this.answer;
   }
   
   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   public String toString ()
   {
      return "{ " + this.nodeId + " ; " + this.answer + " }";
   }
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
