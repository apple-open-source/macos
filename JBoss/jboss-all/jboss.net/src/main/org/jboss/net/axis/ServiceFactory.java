/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: ServiceFactory.java,v 1.1.2.1 2002/09/12 16:18:03 cgjung Exp $

package org.jboss.net.axis;

import org.apache.axis.EngineConfiguration;
import org.apache.axis.client.AxisClient;
import org.apache.axis.client.Service;
import org.apache.axis.configuration.DefaultEngineConfigurationFactory;

import javax.naming.spi.ObjectFactory;
import javax.naming.Reference;
import javax.naming.Context;
import javax.naming.Name;

import java.util.Hashtable;
import java.util.Map;

/**
 * This service factory will reinstall wsdl-generated web service references
 * and re-attach them to the right configurations with the help
 * of pluggable providers.
 * @created  26.04.02
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public class ServiceFactory implements ObjectFactory {

   public Object getObjectInstance(
      Object refObject,
      Name name,
      Context nameCtx,
      Hashtable environment)
      throws Exception {
      Service instance= null;
      if (refObject instanceof Reference) {
         Reference ref= (Reference) refObject;

         Class serviceClass=
            Thread.currentThread().getContextClassLoader().loadClass(
               ref.getClassName());

         instance= (Service) serviceClass.newInstance();

         // try to find the engine that we were attached to
         // in the registry
         EngineConfiguration newEngine=
            getEngineConfiguration(
               (String) ref.get(Constants.CONFIGURATION_CONTEXT).getContent());

         instance.setEngineConfiguration(newEngine);
         // and reinstall the engine
         instance.setEngine(new AxisClient(newEngine));
      }

      return instance;
   }

   /** dedicated configuration provider */
   protected static EngineConfigurationProvider engineConfigurationProvider;

   /** register an engine configuration provider */
   public static void registerEngineConfigurationProvider(EngineConfigurationProvider ecp) {
      synchronized (ServiceFactory.class) {
         engineConfigurationProvider= ecp;
      }
   }

   /** access engine configuration provider */
   public static EngineConfiguration getEngineConfiguration(String context) {
      if (engineConfigurationProvider != null) {
         ClassLoader loader=Thread.currentThread().getContextClassLoader();
         Thread.currentThread().setContextClassLoader(engineConfigurationProvider.getClass().getClassLoader());
         try{ 
            return engineConfigurationProvider.getClientEngineConfiguration(
               context);
         } finally {
            Thread.currentThread().setContextClassLoader(loader);
         }
      } else {
         return new DefaultEngineConfigurationFactory().getClientEngineConfig();
      }
   }

}