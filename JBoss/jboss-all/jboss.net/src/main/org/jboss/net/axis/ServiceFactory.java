/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: ServiceFactory.java,v 1.1.2.2 2003/11/06 15:36:04 cgjung Exp $

package org.jboss.net.axis;

import java.util.Hashtable;

import javax.naming.Context;
import javax.naming.Name;
import javax.naming.Reference;
import javax.naming.spi.ObjectFactory;

import org.apache.axis.EngineConfiguration;
import org.apache.axis.EngineConfigurationFactory;
import org.apache.axis.client.AxisClient;
import org.apache.axis.client.Service;
import org.apache.axis.configuration.EngineConfigurationFactoryFinder;

/**
 * <p>This service factory will reinstall (wsdl-generated) 
 * web service references and re-attach the stubs to the right 
 * configurations with the help of axis engine configuration factory.</p>
 * <p>It is a temporary alternative to the prototypical axis service factory
 * that only deals with untyped services at this point.</p>
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.2 $
 * @since  26.04.02
 */

public class ServiceFactory implements ObjectFactory {

	/* (non-javadoc)
	 * does dereference jndi links consisting of stub classname
	 * and string-based configuration names.
	 */
	public Object getObjectInstance(
		Object refObject,
		Name name,
		Context nameCtx,
		Hashtable environment)
		throws Exception {
		Service instance = null;
		if (refObject instanceof Reference) {
			Reference ref = (Reference) refObject;

			Class serviceClass =
				Thread.currentThread().getContextClassLoader().loadClass(
					ref.getClassName());

			EngineConfigurationFactory factory =
				EngineConfigurationFactoryFinder.newFactory(
					(String) ref
						.get(Constants.CONFIGURATION_CONTEXT)
						.getContent());

			EngineConfiguration engine = null;

			if (factory != null) {
				engine = factory.getClientEngineConfig();
			}

			instance = (Service) serviceClass.newInstance();

			instance.setEngineConfiguration(engine);
			// and reinstall the engine
			instance.setEngine(new AxisClient(engine));
		}

		return instance;
	}

}
