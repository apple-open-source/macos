/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.agent;

// Hermes JMX Packages
import org.jbossmx.cluster.watchdog.Configuration;

import org.jbossmx.cluster.watchdog.util.HermesMachineProperties;

// Standard Java Packages
import java.util.Enumeration;
import java.util.Properties;

/**
 * Class for setting up the RMI Bindings for Hermes + the Configuration.MACHINE_PROPERTIES file,
 * the RMI part is replaced by RegisterActivatableObjects.
 *
 * @author Stacy Curl
 */
public class Setup
{
    /**
     * @param    args
     * @throws Exception
     */
    public static void main(String[] args) throws Exception
    {
        if (args.length != 3 && args.length != 4)
        {
            System.out.println("Usage: Setup <active machine> <failover machine> <component machine> [<project name>]");
            System.exit(1);
        }

        final String activeMachine = args[0];
        final String failoverMachine = args[1];
        final String componentMachine = args[2];
        final String projectName =  (args.length == 4) ? "" : args[3];

        HermesMachineProperties.setMachineName(Configuration.getRmiActiveAgent(projectName),
                                               activeMachine);
        HermesMachineProperties.setMachineName(Configuration.getRmiFailoverAgent(projectName),
                                               failoverMachine);
        HermesMachineProperties.setMachineName(Configuration.getRmiComponentAgent(projectName),
                                               componentMachine);

        /*
        Configuration.setMachineName(Configuration.ACTIVE_MACHINE_PROPERTY,
            Configuration.DEFAULT_ACTIVE_MACHINE);
        Configuration.setMachineName(Configuration.FAILOVER_MACHINE_PROPERTY,
            Configuration.DEFAULT_FAILOVER_MACHINE);
        Configuration.setMachineName(Configuration.COMPONENT_MACHINE_PROPERTY,
            Configuration.DEFAULT_COMPONENT_MACHINE);
        */

        HermesMachineProperties.setProperty(Configuration.DEFAULT_ACTIVE_MACHINE_PROPERTY,
                                            activeMachine);
        HermesMachineProperties.setProperty(Configuration.DEFAULT_FAILOVER_MACHINE_PROPERTY,
                                            failoverMachine);
        System.exit(0);
    }
}


/*--- Formatted in Stacy XCT Convention Style on Thu, Feb 15, '01 ---*/


/*------ Formatted by Jindent 3.22 Basic 1.0 --- http://www.jindent.de ------*/
