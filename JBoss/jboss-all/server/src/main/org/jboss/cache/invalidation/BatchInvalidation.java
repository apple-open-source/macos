/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.cache.invalidation;

import java.io.Serializable;

/**
 * Structure that contains keys to be invalidated and the name of the group
 * on which these invalidation must be performed.
 *
 * @see InvalidationManagerMBean
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>24 septembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class BatchInvalidation implements java.io.Serializable
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   protected Serializable[] ids = null;
   protected String invalidationGroupName = null;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public BatchInvalidation ()
   {
   }
   
   public BatchInvalidation (Serializable[] ids, String invalidationGroupName)
   {
      this.ids = ids;
      this.invalidationGroupName = invalidationGroupName;
   }
   
   public Serializable[] getIds ()
   {
      return this.ids;
   }
   
   public void setIds (Serializable[] ids)
   {
      this.ids = ids;
   }
   
   public String getInvalidationGroupName ()
   {
      return invalidationGroupName;
   }
   
   public void setInvalidationGroupName (String invalidationGroupName)
   {
      this.invalidationGroupName = invalidationGroupName;
   }
   
   // Public --------------------------------------------------------
   
   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
