/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.services.binding;

/** A factory interface for obtaining ServicesStore instances
 *
 * @version $Revision: 1.1 $
 * @author Scott.Stark@jboss.org
 */
public interface ServicesStoreFactory 
{
   /** Load the contents of a store.
    */
   public ServicesStore newInstance();
}
