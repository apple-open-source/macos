/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: SetClassLoaderHandler.java,v 1.1.4.1 2002/09/12 16:18:03 cgjung Exp $

package org.jboss.net.axis;

import org.apache.axis.MessageContext;
import org.apache.axis.EngineConfiguration;

import javax.xml.namespace.QName;

/**
 * This handler is to embed an incoming request into
 * the right classloader and should be put into a request
 * chain after the service detection handlers. It should be
 * complemented by a seperate 
 * <code>org.jboss.net.axis.ResetClassLoaderHandler</code>
 * in the response chain to reinstall the thread association
 * in case of success.
 * @created  11.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.4.1 $
 */

public class SetClassLoaderHandler extends ResetClassLoaderHandler {

	//
	// API
	//
	
   /**
    * reroutes the thread associated classloader to
    * the one that deployed the corresponding service
    * @see Handler#invoke(MessageContext)
    */
   public void invoke(MessageContext msgContext) {
      EngineConfiguration engineConfig=msgContext.getAxisEngine().getConfig();
	  
	  if(engineConfig instanceof XMLResourceProvider) {
		XMLResourceProvider config=(XMLResourceProvider)  engineConfig;
		ClassLoader newLoader=config.getMyDeployment().getClassLoader(new QName(null,
			msgContext.getTargetService()));
		if(newLoader!=null) {
		   ClassLoader currentLoader=Thread.currentThread().getContextClassLoader();
		   if(!newLoader.equals(currentLoader)) {
			   msgContext.setProperty(Constants.OLD_CLASSLOADER_PROPERTY,currentLoader);
			   Thread.currentThread().setContextClassLoader(newLoader);
		   }
		}	
	  }
   }	  
		   
}
