/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.services.binding;

import java.net.URL;
import javax.management.ObjectName;

/** Interface for API to persist, read, and look up service configs
 *
 * @version $Revision: 1.2.2.1 $
 * @author <a href="mailto:bitpushr@rochester.rr.com">Mike Finn</a>.
 * @author Scott.Stark@jboss.org
 */
public interface ServicesStore 
{
   /** Load the contents of a store.
    * @param storeURL the URL representing the location of the store
    * @exception Exception thrown on any failure to load the store
    */
   public void load(URL storeURL) throws Exception;
   /** Save the current store contents
    * @param storeURL the URL representing the location of the store
    * @exception Exception thrown on any failure to save the store
    */
   public void store(URL storeURL) throws Exception;

   /** Obtain a ServiceConfig object for the given server instance and target
    * service JMX ObjectName. This is called by the JBoss service configuration
    * layer to obtain service attribute binding overrides.
    *
    * @param serverName the name identifying the JBoss server instance in
    *    which the service is running.
    * @param serviceName the JMX ObjectName of the service
    * @return The ServiceConfig if one exists for the <serverName, serviceName>
    *    pair, null otherwise.
    */
   public ServiceConfig getService(String serverName, ObjectName serviceName);

   /** Add a ServiceConfig to the store. This is an optional method not used
    * by the JBoss service configuration layer.
    *
    * @param serverName the name identifying the JBoss server instance in
    *    which the service is running.
    * @param serviceName the JMX ObjectName of the service
    * @param serviceConfig the configuration to add
    * @throws DuplicateServiceException thrown if a configuration for the
    *    <serverName, serviceName> pair already exists.
    */
   public void addService(String serverName, ObjectName serviceName,
      ServiceConfig serviceConfig)
      throws DuplicateServiceException;

   /** Remove a service configuration from the store. This is an optonal method
    * not used by the JBoss service configuration layer.
    *
    * @param serverName the name identifying the JBoss server instance in
    *    which the service is running.
    * @param serviceName the JMX ObjectName of the service
    */
   public void removeService(String serverName, ObjectName serviceName);
}
