/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.cache.invalidation.bridges;

import org.jboss.cache.invalidation.BatchInvalidation;

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
 * <p><b>30. septembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class JMSCacheInvalidationMessage
   implements java.io.Serializable
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   protected BatchInvalidation[] bis = null;
   protected java.rmi.dgc.VMID emitter = null;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public JMSCacheInvalidationMessage (java.rmi.dgc.VMID source, 
                                       String groupName, 
                                       java.io.Serializable[] keys)
   {
      this.emitter = source;
      this.bis = new BatchInvalidation[] 
      {
         new BatchInvalidation (keys, groupName)
      };
   }
   
   public JMSCacheInvalidationMessage (java.rmi.dgc.VMID source, 
                                       BatchInvalidation[] invalidations)
   {
      this.emitter = source;
      this.bis = invalidations;
   }
   
   // Public --------------------------------------------------------
   
   public BatchInvalidation[] getInvalidations()
   {
      if (this.bis == null)
         this.bis = new BatchInvalidation[0];
      
      return this.bis;
   }
   
   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
