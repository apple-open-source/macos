/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog;

import java.util.Properties;

import java.rmi.*;

/**
 * Class for storing hard coded configuration for Hermes.
 *
 * @author Stacy Curl
 */
final public class Configuration
{
    /** The property name which refers to the Jar path the MLet MBean should used to look for MBeans */
    public static final String MLET_JAR_PATH_PROPERTY = "MLET_JAR_PATH";
    /** The property name which refers to the MLeRMIt resource location */
    public static final String MLET_RESOURCE_LOCATION_PROPERTY = "MLET_RESOURCE_LOCATION";

    /** The property name which refers the whether failure in loading / starting System mbeans
     *  is tolerated. A System MBean is HTMLAdaptor server, RMIAdaptorService */
    public static final String TOLERATE_SYSTEM_MBEAN_FAILURE_PROPERTY =
        "TOLERATE_SYSTEM_MBEAN_FAILURE";
    /** The property name which refers the whether failure in loading / starting Custom mbeans
     *  is tolerated */
    public static final String TOLERATE_CUSTOM_MBEAN_FAILURE_PROPERTY =
        "TOLERATE_CUSTOM_MBEAN_FAILURE";

    /** The property name which refers to the class that will configure the agent */
    public static final String AGENT_CONFIGURATOR_CLASS_PROPERTY = "AGENT_CONFIGURATOR_CLASS";

    /** */
    private static final String DEFAULT_HERMES_DIRECTORY = "/apps/hermes";
    /** */
    public static final String HERMES_DIRECTORY_PROPERTY = "HERMES_DIRECTORY";

    /** The root directory of the Hermes deployment */
    public static final String HERMES_DIRECTORY;

    static
    {
        final Properties properties = System.getProperties();
        HERMES_DIRECTORY = properties.getProperty(HERMES_DIRECTORY_PROPERTY,
                                                  DEFAULT_HERMES_DIRECTORY);
    }

    /** The directory in which Hermes configuration is stored */
    public static final String HERMES_CONFIG_DIRECTORY = HERMES_DIRECTORY + "/config";

    /** The location of the file which stores the machines the JMX Agents are running on */
    public static final String MACHINE_PROPERTIES = HERMES_CONFIG_DIRECTORY + "/machine.properties";

    /** */
    private static final String ACTIVE_AGENT = "ActiveAgent";
    /** */
    private static final String FAILOVER_AGENT = "FailoverAgent";
    /** */
    private static final String COMPONENT_AGENT = "ComponentAgent";

    /** The RMI Binding of the {@link SwapMachines} remote interface */
    private static final String SWAPMACHINES = "SwapMachines";

    /**
     * @param    projectName
     *
     * @return
     */
    public static final String getRmiActiveAgent(String projectName)
    {
        return "/" + getRmiProjectPrefix(projectName) + ACTIVE_AGENT;
    }

    /**
     * @param    projectName
     *
     * @return
     */
    public static final String getRmiFailoverAgent(String projectName)
    {
        return "/" + getRmiProjectPrefix(projectName) + FAILOVER_AGENT;
    }

    /**
     * @param    projectName
     *
     * @return
     */
    public static final String getRmiComponentAgent(String projectName)
    {
        return "/" + getRmiProjectPrefix(projectName) + COMPONENT_AGENT;
    }

    /**
     * @param    projectName
     *
     * @return
     */
    public static final String getRmiSwapMachines(String projectName)
    {
        return "/" + getRmiProjectPrefix(projectName) + SWAPMACHINES;
    }

    /**
     * @param    projectName
     *
     * @return
     */
    private static final String getRmiProjectPrefix(String projectName)
    {
        return ((projectName != null) && (projectName.length() != 0))
               ? (projectName + "-")
               : "";
    }

    /** Default Active Machine property name in MACHINE_PROPERTIES */
    public static final String DEFAULT_ACTIVE_MACHINE_PROPERTY = "DefaultActiveMachine";
    /** Default Failover Machine property name in MACHINE_PROPERTIES */
    public static final String DEFAULT_FAILOVER_MACHINE_PROPERTY = "DefaultFailoverMachine";

    /** The Default machine on which to perform Active monitoring */
    private static final String DEFAULT_ACTIVE_MACHINE = "hugin";
    /** The Default machine to failover to */
    private static final String DEFAULT_FAILOVER_MACHINE = "munin";
}
