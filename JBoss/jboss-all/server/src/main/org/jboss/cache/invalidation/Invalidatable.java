/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.cache.invalidation;

import java.io.Serializable;

/**
 * Represent an invalidable resource, such as a cache
 * @see InvalidationGroup
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>21. septembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface Invalidatable
{
   /**
    * Indicates that the resource with the given key should be invalidated (i.e. removed
    * from cache)
    * @param key Key of the resource to be invalidated
    */   
   public void isInvalid (Serializable key);   
   
   /** Indicates that the resources with the give keys should be invalidated (i.e.
    * removed from cache)
    *
    * @param keys Keys of the resources to be invalidated
    */   
   public void areInvalid (Serializable[] keys);   

}