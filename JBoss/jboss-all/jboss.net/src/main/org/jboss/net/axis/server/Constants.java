/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: Constants.java,v 1.12.2.2 2003/04/26 09:05:23 cgjung Exp $

package org.jboss.net.axis.server;

/**
 * Some Constants for the server package
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @since 28. September 2001
 * @version $Revision: 1.12.2.2 $
 */

public interface Constants extends org.jboss.net.Constants {

	/** programmatic constants */
	static final String DOMAIN = "jboss.net";
	static final String NAME = "Axis";
	static final String TYPE = "service";
	static final String SERVER_DELEGATE_NAME ="JMImplementation:type=MBeanServerDelegate";
	static final String SERVER_ID_ATTRIBUTE = "MBeanServerId";
	static final String AXIS_DEPLOYMENT_DESCRIPTOR = "META-INF/install-axis.xml";
	static final String AXIS_DEPLOY_DIR = "_axis_";
	static final String WEB_DEPLOYMENT_DESCRIPTOR = "/WEB-INF/web.xml";
	static final String JBOSS_WEB_DEPLOYMENT_DESCRIPTOR = "/WEB-INF/jboss-web.xml";
	static final String DEFAULT_ROOT_CONTEXT = "axis";
	static final String WSR_FILE_EXTENSION = ".wsr";
	static final String XML_FILE_EXTENSION = ".xml";
	static final String AXIS_ENGINE_ATTRIBUTE = "AxisEngine";
	static final String GET_AXIS_SERVER_METHOD_NAME = "getAxisServer";
	static final String AXIS_CONFIGURATION_FILE = "axis-config.xml";
	static final String AXIS_CLIENT_CONFIGURATION_FILE = "client-config.xml";
	static final String WEB_SERVICE_DESCRIPTOR = "META-INF/web-service.xml";
	static final String USER_TRANSACTION_JNDI_NAME = "UserTransaction";

	/** constants referring to options in the axis messagecontext or handler options */
	static final String ALLOWED_ROLES_OPTION = "allowedRoles";
	static final String DENIED_ROLES_OPTION = "deniedRoles";
	static final String SECURITY_DOMAIN_OPTION = "securityDomain";
        static final String VALIDATE_UNAUTHENTICATED_CALLS_OPTION = "validateUnauthenticatedCalls";
	static final String TRANSACTION_PROPERTY = "transaction";

	/** message id constants are english raw messages at the same time */
	static final String AXIS_DEPLOYMENT_DESCRIPTOR_NOT_FOUND =
		"The axis deployment descriptor is lacking in the service archive!";
	static final String ABOUT_TO_DEPLOY_0_UNDER_CONTEXT_1 =
		"About to deploy axis web application from {0} under context {1}.";
	static final String AXIS_ALREADY_STARTED = "Axis has already been started.";
	static final String ABOUT_TO_UNDEPLOY_0 =
		"About to undeploy axis web application from {0}.";
	static final String COULD_NOT_STOP_AXIS = "Could not correctly stop Axis.";
	static final String AXIS_ALREADY_STOPPED = "Axis has already been stopped.";
	static final String SET_WAR_DEPLOYER_0 = "Seting WarDeployerName to {0}.";
	static final String SET_ROOT_CONTEXT_0 = "Seting RootContext to {0}.";
	static final String SET_SECURITY_DOMAIN_TO_0 =
		"Setting Security Domain to {0}.";
	static final String ABOUT_TO_CREATE_AXIS_0 =
		"About to deploy axis descriptor {0}, create step.";
	static final String ABOUT_TO_START_AXIS_0 =
		"About to deploy axis descriptor {0}, start step.";
	static final String ABOUT_TO_STOP_AXIS_0 =
		"About to undeploy axis descriptor {0}, stop step.";
	static final String ABOUT_TO_DESTROY_AXIS_0 =
		"About to undeploy axis descriptor {0}, destroy step.";
	static final String COULD_NOT_DEPLOY_DESCRIPTOR =
		"Could not deploy axis descriptor.";
	static final String COULD_NOT_FIND_AXIS_CONFIGURATION_0 =
		"Could not find the axis configuration file {0}.";
	static final String NO_VALID_WEB_SERVICE_DESCRIPTOR =
		"Could not find a valid web service descriptor.";
	static final String COULD_NOT_DEPLOY = "Could not deploy url.";
	static final String COULD_NOT_UNDEPLOY = "Could not undeploy url.";
	static final String COULD_NOT_COPY_URL = "Could not download url.";
	static final String CANNOT_CHANGE_ROOT_CONTEXT =
		"Cannot change root context while service is running. Stop first.";
	static final String AXIS_SERVER_CONTEXT_OCCUPIED =
		"There is already an Axis service running under that root context.";
	static final String EJB_REF_MUST_HAVE_UNIQUE_NAME =
		"An ejb-ref element must have a unique ejb-ref-name element.";
	static final String EJB_REF_MUST_HAVE_UNIQUE_LINK =
		"An ejb-ref element must have a unique ejb-link element.";
	static final String CANNOT_FIND_WEB_DEPLOYER =
		"Could not find a suitable web container.";
	final static String ERR_EXISTING_HEADER =
		"HttpServletRequest already contains a SOAPAction header.";
	final static String ERR_MISSING_PARM =
		"HttpServletRequest does not contain a SOAPAction parameter.";
	
	/** 
	 * property key behind which an action handler can mask its presence in the transport chain
	 * to influence wsdl generation
	 */
	final static String ACTION_HANDLER_PRESENT_PROPERTY="org.jboss.net.axis.server.ACTION_HANDLER_PRESENT";
	
}
