/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.metadata;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;
import org.jboss.cache.invalidation.InvalidationManager;

/**
 * Manages attributes related to distributed (possibly local-only)
 * cache invalidations
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>26 septembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class CacheInvalidationConfigMetaData extends MetaData
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   protected String invalidationGroupName = null;
   protected String cacheInvaliderObjectName = null;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public CacheInvalidationConfigMetaData () { super (); }
   
   // Public --------------------------------------------------------
   
   public String getInvalidationGroupName ()
   {
      return this.invalidationGroupName;
   }
   
   public String getInvalidationManagerName ()
   {
      return this.cacheInvaliderObjectName;
   }
   
   public void init(BeanMetaData data)
   {
      // by default we use the bean name as the group name
      //
      this.invalidationGroupName = data.getEjbName ();
      
      this.cacheInvaliderObjectName = InvalidationManager.DEFAULT_JMX_SERVICE_NAME;
   }
   
   public void importJbossXml(Element element) throws DeploymentException 
   {
      this.invalidationGroupName = getElementContent(getOptionalChild(element, "invalidation-group-name"), this.invalidationGroupName);
      this.cacheInvaliderObjectName = getElementContent(getOptionalChild(element, "invalidation-manager-name"), this.cacheInvaliderObjectName);
   }

   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
