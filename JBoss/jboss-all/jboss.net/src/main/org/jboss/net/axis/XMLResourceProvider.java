/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: XMLResourceProvider.java,v 1.4.2.2 2002/11/20 04:00:56 starksm Exp $

package org.jboss.net.axis;

import org.apache.axis.AxisEngine;
import org.apache.axis.ConfigurationException;
import org.apache.axis.configuration.FileProvider;
import org.apache.axis.deployment.wsdd.WSDDGlobalConfiguration;
import org.apache.axis.utils.XMLUtils;

import java.net.URL;
import java.io.InputStream;
import java.io.IOException;

/**
 * A <code>FileProvider</code> that sits on a given URL and
 * that hosts classloader-aware deployment information.
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @created 28. September 2001
 * @version $Revision: 1.4.2.2 $
 */

public class XMLResourceProvider extends FileProvider {

	//
	// Attributes
	//

	/** the original resource that we host */
	final protected URL resource;

   /** input stream cache */
   protected InputStream is;
   
	//
	// Constructors
	//

	/**
	 * construct a new XmlResourceProvider
	 * @param resource url pointing to the deployment descriptor
	 */
	public XMLResourceProvider(URL resource) {
		super((InputStream) null);
		this.resource = resource;
	}

	//
	// Public API
	//

   /** override input stream setter to sync protected cache */
   public void setInputStream(InputStream stream) {
      super.setInputStream(stream);
      is=stream;
   }
   
	/** configures the given AxisEngine with the given descriptor */
	public void configureEngine(AxisEngine engine) throws ConfigurationException {
		buildDeployment();
		getDeployment().configureEngine(engine);
		engine.refreshGlobalOptions();
	}

	/** constructs a new deployment */
	public synchronized Deployment buildDeployment()
		throws ConfigurationException {
		if (getDeployment() == null) {
			try {
				if (is == null) {
					setInputStream(resource.openStream());
				}

				setDeployment(
					new Deployment(XMLUtils.newDocument(is).getDocumentElement())
            );
            
				setInputStream(null);

            if(getDeployment().getGlobalConfiguration()==null) {
               WSDDGlobalConfiguration config=new WSDDGlobalConfiguration();
               config.setOptionsHashtable(new java.util.Hashtable());
               getDeployment().setGlobalConfiguration(config);
            }
            				
			} catch (Exception e) {
				throw new ConfigurationException(e);
			}
		}
		return getMyDeployment();
	}

	/** returns out special deployment */
	public Deployment getMyDeployment() {
		return (Deployment) getDeployment();
	}

	/** not supported, yet. Should we use http-push or what? */
	public void writeEngineConfig(AxisEngine engine) {
		// NOOP
	}

}