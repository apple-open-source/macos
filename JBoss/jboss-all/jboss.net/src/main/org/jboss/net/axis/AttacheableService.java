/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AttacheableService.java,v 1.1.2.2 2003/11/06 15:36:04 cgjung Exp $

package org.jboss.net.axis;

import javax.naming.Reference;
import javax.naming.Referenceable;
import javax.naming.StringRefAddr;

/**
 * <p>The attacheable service implementation 
 * allows to bind axis service instances into JNDI 
 * without loosing the configuration link to their engine/deployer.</p> 
 * <p>Configuration pointers are reinstalled 
 * by a dedicated context attribute that allows to identify a 
 * target configuration when deserialized by the associated 
 * ServiceFactory.</p>
 * @see ServiceFactory
 * @since  26.04.02
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.2 $
 */

public class AttacheableService implements Referenceable {

	//
	// Attributes
	//

	/** 
	 * the real axis service implementation that 
	 * regularly looses its configuration
	 */

	protected String serviceClass;

	/** this is what we need to find the important part, which is the
	 *  engine configuration, again. 
	 */
	protected String rootContext;

	//
	// Constructors
	//

	/** 
	 * Creates a new ServiceFactory.
	 * @param service the axis service
	 */

	public AttacheableService(String serviceClass, String rootContext) {
		this.serviceClass = serviceClass;
		this.rootContext = rootContext;
	}

	//
	// Naming API
	//

	public Reference getReference() {
		Reference myRef =
			new Reference(serviceClass, ServiceFactory.class.getName(), null);
		myRef.add(new StringRefAddr(Constants.CONFIGURATION_CONTEXT, rootContext));
		return myRef;
	}

}
