/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: ServiceClassLoaderAwareWSDDHandlerProvider.java,v 1.3.2.1 2003/11/06 15:36:04 cgjung Exp $
package org.jboss.net.axis;

import org.apache.axis.EngineConfiguration;
import org.apache.axis.Handler;
import org.apache.axis.deployment.wsdd.WSDDService;
import org.apache.axis.deployment.wsdd.providers.WSDDHandlerProvider;

/**
 * A subclass of the official handler provider that
 * is able to load the specified handler classes from
 * the service context classloader.
 * @author jung
 * @since 12.03.2003
 * @version $Revision: 1.3.2.1 $
 */
public class ServiceClassLoaderAwareWSDDHandlerProvider
   extends WSDDHandlerProvider {

   /* (non-Javadoc)
    * @see org.apache.axis.deployment.wsdd.WSDDProvider#newProviderInstance(org.apache.axis.deployment.wsdd.WSDDService, org.apache.axis.EngineConfiguration)
    */
   public Handler newProviderInstance(
      WSDDService arg0,
      EngineConfiguration arg1)
      throws Exception {
      if(arg1 instanceof Deployment) {
      	Deployment deployment=(Deployment) arg1;
      	ClassLoader loader=deployment.getClassLoader(arg0.getQName());
      	ClassLoader old=Thread.currentThread().getContextClassLoader();
      	Thread.currentThread().setContextClassLoader(loader);
      	try{
      		return super.newProviderInstance(arg0,arg1);
      	} finally {
      		Thread.currentThread().setContextClassLoader(old);
      	}
      } else {
      	return super.newProviderInstance(arg0, arg1);
      }
   }

}
