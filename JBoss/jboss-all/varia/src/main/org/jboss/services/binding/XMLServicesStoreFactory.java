/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.services.binding;

/** A factory interface for obtaining XMLServicesStore instances
 *
 * @version $Revision: 1.1 $
 * @author Scott.Stark@jboss.org
 */
public class XMLServicesStoreFactory implements ServicesStoreFactory
{
   /** Load the contents of a store.
    */
   public ServicesStore newInstance()
   {
      return new XMLServicesStore();
   }
}
