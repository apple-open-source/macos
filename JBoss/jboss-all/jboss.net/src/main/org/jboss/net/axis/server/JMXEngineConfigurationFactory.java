/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: JMXEngineConfigurationFactory.java,v 1.2.2.1 2003/11/06 15:36:05 cgjung Exp $

package org.jboss.net.axis.server;

import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.MBeanServer;

import org.apache.axis.EngineConfiguration;
import org.apache.axis.EngineConfigurationFactory;
import org.apache.axis.server.AxisServer;
import org.jboss.mx.util.MBeanServerLocator;

/**
 * <p> A configuration factory that accesses axis server engines 
 * via JMX attribute access. </p>
 * @author jung
 * @version $Revision: 1.2.2.1 $
 * @since 9.9.2002
 */

public class JMXEngineConfigurationFactory
	implements EngineConfigurationFactory {

	//
	// Attributes
	//

	protected ObjectName objectName;
	protected MBeanServer server;

	//
	// Constructors
	//

	/** construct a new factory tied to a particular engine provider mbean */

	protected JMXEngineConfigurationFactory(String name)
		throws MalformedObjectNameException {
		server = MBeanServerLocator.locateJBoss();
		this.objectName = new ObjectName(name);
	}

	//
	// Protected Helpers
	//

	/**
	 * find attribute through JMX server and mbean
	 */

	protected Object getAttribute(String attributeName) {
		try {
			return server.getAttribute(objectName, attributeName);
		} catch (JMException e) {
			return null;
		}
	}

	//
	// Public API
	//

	/** return axis server associated with mbean */
	public AxisServer getAxisServer() {
		return (AxisServer) getAttribute("AxisServer");
	}

	/* (non-Javadoc)
		* @see org.apache.axis.EngineConfigurationFactory#getClientEngineConfig()
		*/
	public EngineConfiguration getClientEngineConfig() {
		return (EngineConfiguration) getAttribute("ClientEngineConfiguration");
	}

	/* (non-Javadoc)
		* @see org.apache.axis.EngineConfigurationFactory#getServerEngineConfig()
		*/
	public EngineConfiguration getServerEngineConfig() {
		return (EngineConfiguration) getAttribute("ServerEngineConfiguration");
	}

	/**
		* static method to create a new jmx factory
		* @param param objectname of the server mbean
		* @return a new factory bound to that mbean, if it exists
		*/
	public static JMXEngineConfigurationFactory newJMXFactory(String param) {
		try {
			return new JMXEngineConfigurationFactory((String) param);
		} catch (MalformedObjectNameException e) {
			return null;
		}
	}

	/**
	 * static method to create a new factory along the Axis spec
	 * @param param specification of the configuration
	 * @return new factory, if the param represents an mbean object name
	 */

	public static EngineConfigurationFactory newFactory(Object param) {
		if (param instanceof String) {
			return newJMXFactory((String) param);
		} else {
			return null;
		}
	}

}
