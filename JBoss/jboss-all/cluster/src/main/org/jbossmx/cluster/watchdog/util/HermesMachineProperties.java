/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util;

// Hermes JMX Packages
import org.jbossmx.cluster.watchdog.Configuration;
import org.jboss.logging.Logger;

// Standard Java Packages
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

import java.util.HashSet;
import java.util.Iterator;
import java.util.Properties;
import java.util.Set;


/**
 * Utility class for retrieving Hermes properties from a Java Properties file
 *
 * @author Stacy Curl
 */
public class HermesMachineProperties
{

    /**
     * Get the machine that odin points to
     *
     * @return the machine that odin points to
     */
//    public static String getOdin()
//    {
//        return getMachineName(Configuration.ACTIVE_AGENT);
//    }

    /**
     * Gets the machine that <code>rmiAgentBinding</code> is running on
     *
     * @param    rmiAgentBinding the RMI Binding of the JMX Agent to find
     *
     * @return the machine that <code>rmiAgentBinding</code> is running on
     */
    public static String getMachineName(String rmiAgentBinding)
    {
        return getProperty(getMachineProperty(rmiAgentBinding));
    }

    /**
     * Sets the machine that <code>rmiAgentBinding</code> is running on
     *
     * @param    rmiAgentBinding rmiAgentBinding the RMI Binding of the JMX Agent
     * @param    machineName the machine that <code>rmiAgentBinding</code> is running on
     *
     * @return whether the <code>machineName</code> was successfully stored.
     */
    public static boolean setMachineName(String rmiAgentBinding, String machineName)
    {
        return setProperty(getMachineProperty(rmiAgentBinding), machineName);
    }

    /**
     * Gets the Watcher details for <code>rmiAgentBinding</code>
     *
     * @param    rmiAgentBinding the RMI Binding of the JMX Agent to query
     *
     * @return an array containing two properties
     * <p>1) The RMI Binding of the JMX Agent containing the Watchdog MBean watching
     * <code>rmiAgentBinding</code>
     * <p>2) The ObjectName of the Watchdog MBean watching <code>rmiAgentBinding</code>
     */
    public static String[] getAgentWatcherDetails(String rmiAgentBinding)
    {
        String[] watcherDetails = new String[2];

        watcherDetails[0] = getProperty(getWatcherRmiBindingProperty(rmiAgentBinding));
        watcherDetails[1] = getProperty(getWatcherObjectNameProperty(rmiAgentBinding));

        return watcherDetails;
    }

    /**
     * Sets the Watcher details for <code>rmiAgentBinding</code>
     *
     * @param    rmiAgentBinding the RMI Binding of the IMX Agent to update
     * @param    watcherAgentRmiBinding the RMI Binding of the JMX Agent containing the Watchdog
     * MBean watching <code>rmiAgentBinding</code>
     * @param    watcherObjectName the ObjectName of the Watchdog MBean watching
     * <code>rmiAgentBinding</code>
     *
     * @return whether the watcher details were updated for <code>rmiAgentBinding</code>
     */
    public static boolean setAgentWatcherDetails(String rmiAgentBinding,
        String watcherAgentRmiBinding, String watcherObjectName)
    {
        return setProperty(getWatcherRmiBindingProperty(rmiAgentBinding), watcherAgentRmiBinding)
               && setProperty(getWatcherObjectNameProperty(rmiAgentBinding), watcherObjectName);
    }

    /**
     * Gets the value of the <code>propertyName</code> property
     *
     * @param    propertyName the property to obtain
     *
     * @return the value of the <code>propertyName</code> property
     */
    public static String getProperty(String propertyName)
    {
        String property = null;

        Properties properties = loadProperties(Configuration.MACHINE_PROPERTIES);

        if(properties != null)
        {
            property = properties.getProperty(propertyName);
        }

        if(property == null)
        {
            listFile(Configuration.MACHINE_PROPERTIES);
        }

        return property;
    }

    /**
     * Sets the value of the <code>propertyName</code> property
     *
     * @param    property the property to update
     * @param    value the value of the property
     *
     * @return whether the property was updated
     */
    public static boolean setProperty(String property, String value)
    {
        Properties properties = loadProperties(Configuration.MACHINE_PROPERTIES);

        if(properties != null)
        {
            properties.setProperty(property, value);

            return saveProperties(properties, Configuration.MACHINE_PROPERTIES);
        }
        else
        {
            return false;
        }
    }

    /**
     * Gets the name of the property used to store the current machine running the JMX Agent bound
     * to <code>rmiAgentBinding</code>
     *
     * @param    rmiAgentBinding the JMX Agent RMI Binding
     *
     * @return the name of the property used to store the current machine running the JMX Agent
     * bound to <code>rmiAgentBinding</code>
     */
    public static String getMachineProperty(String rmiAgentBinding)
    {
        return rmiAgentBinding + ".Machine";
    }

    /**
     * Gets the name of the property used to store the RMI Binding of the Agent watching the JMX
     * Agent bound to <code>rmiAgentBinding</code>
     *
     * @param    rmiAgentBinding the JMX Agent RMI Binding
     *
     * @return the name of the property used to store the RMI Binding of the Agent watching the JMX
     * Agent bound to <code>rmiAgentBinding</code>
     */
    public static String getWatcherRmiBindingProperty(String rmiAgentBinding)
    {
        return rmiAgentBinding + ".Watcher.RmiBinding";
    }

    /**
     * Gets the name of the property used to store the ObjectName of the Watchdog MBean watching the
     * JMX Agent bound to <code>rmiAgentBinding</code>
     *
     * @param    rmiAgentBinding the JMX Agent RMI Binding
     *
     * @return the name of the property used to store the ObjectName of the Watchdog MBean watching the
     * JMX Agent bound to <code>rmiAgentBinding</code>
     */
    public static String getWatcherObjectNameProperty(String rmiAgentBinding)
    {
        return rmiAgentBinding + ".Watcher.ObjectName";
    }

    /**
     * Gets the RMI Bindings of all the JMX Agents running on <code>machineName</code>
     *
     * @param    machineName the name of the machine to search for JMX Agents
     *
     * @return the RMI Bindings of all the JMX Agents running on <code>machineName</code>
     */
    public static Set getAgentsRunningOnMachine(final String machineName)
    {
        Set agents = null;

        Properties properties = loadProperties(Configuration.MACHINE_PROPERTIES);

        if(properties != null)
        {
            final Set keySet = properties.keySet();

            for(Iterator i = keySet.iterator(); i.hasNext(); )
            {
                final String nextKey = (String) i.next();

                if(isMachineProperty(nextKey)
                    && properties.getProperty(nextKey).equals(machineName))
                {
                    if(agents == null)
                    {
                        agents = new HashSet();
                    }

                    agents.add(getAgentName(nextKey));
                }
            }
        }

        return agents;
    }

    /**
     * Gets whether <code>key</code> is a valid property key for storing the machine on which a JMX
     * Agent is running
     *
     * @param    key
     *
     * @return whether <code>key</code> is a valid property key for storing the machine on which a JMX
     * Agent is running
     */
    private static boolean isMachineProperty(final String key)
    {
        return ((key != null) && (key.indexOf(".Machine") != -1));
    }

    /**
     * Gets the substring of <code>key</code> which refers to the Agents RMI Binding
     *
     * @param    key
     *
     * @return the substring of <code>key</code> which refers to the Agents RMI Binding
     */
    private static String getAgentName(final String key)
    {
        final int indexOfMachine = key.indexOf(".Machine");

        return key.substring(0, indexOfMachine);
    }

    /**
     * Gets the resolved RMI Binding for <code>unresolvedRmiAgentBinding</code> by looking up
     * the machine which is running the Agent implictly refered to in
     * <code>unresolvedRmiAgentBinding</code>
     * <p> Example: Unresolved 'RMI Binding': {/ActiveAgent}, resolved RMI Binding:
     * rmi://hugin/ActiveAgent
     *
     * @param    unresolvedRmiAgentBinding the RMI Binding to resolve
     *
     * @return the resolved RMI Binding
     */
    public static String getResolvedRmiAgentBinding(String unresolvedRmiAgentBinding)
    {
        String resolvedRmiAgentBinding;

        // Format //machineName/binding
        // Format {/binding}
        //   -> //Configuration.getMachine("/binding") + "/binding"

        if(unresolvedRmiAgentBinding.indexOf("{") == -1)
        {
            resolvedRmiAgentBinding = unresolvedRmiAgentBinding;
        }
        else
        {
            int leftBrace = unresolvedRmiAgentBinding.indexOf("{");
            int rightBrace = unresolvedRmiAgentBinding.indexOf("}");

            final String rmiAgentBinding = unresolvedRmiAgentBinding.substring(leftBrace + 1,
                                               rightBrace);

            final String machineName = getMachineName(rmiAgentBinding);

            if(machineName != null)
            {
                resolvedRmiAgentBinding = "//" + machineName + rmiAgentBinding;
            }
            else
            {
                resolvedRmiAgentBinding = null;
            }
        }

        LOG.debug("getResolvedRmiAgentBinding(" + unresolvedRmiAgentBinding + ") = "
                  + resolvedRmiAgentBinding);

        return resolvedRmiAgentBinding;
    }

    /**
     * Removes the leftmost and rightmost braces in <code>input</code>
     *
     * @param    input the String to remove braces from
     *
     * @return the leftmost and rightmost braces in <code>input</code>
     */
    public static String removeBraces(String input)
    {
        final int leftBrace = input.indexOf("{");
        final int rightBrace = input.indexOf("}");

        return input.substring(leftBrace + 1, rightBrace);
    }

    /**
     * Prints out all the properties in <code>properties</code>
     *
     * @param    properties the Properties to list
     */
    public static void enumerateProperties(Properties properties)
    {
        System.out.println("enumerateProperties");

        Set keySet = properties.keySet();

        for(Iterator i = keySet.iterator(); i.hasNext(); )
        {
            String nextKey = (String) i.next();

            System.out.println("Prop(" + nextKey + ") = " + properties.getProperty(nextKey));
        }

        System.out.println("enumerateProperties - done");
    }

    /**
     * Returns a Properties class loaded from a properties file
     *
     * @param    fileName the properties file to load
     *
     * @return a Properties class loaded from a properties file
     */
    private static Properties loadProperties(final String fileName)
    {
        Properties properties = null;

        try
        {
            InputStream is = new FileInputStream(fileName);

            properties = new Properties();

            properties.load(is);

            is.close();
        }
        catch(Exception e)
        {
            e.printStackTrace(System.out);

            properties = null;
        }

        return properties;
    }

    /**
     * Saves a Properties object to a file
     *
     * @param    properties the Properties object to save
     * @param    fileName the file to save into
     *
     * @return whether <code>properties</code> was saved successfully
     */
    private static boolean saveProperties(final Properties properties, final String fileName)
    {
        boolean saved = false;

        try
        {
            OutputStream os = new FileOutputStream(fileName);
            properties.store(os, null);
            os.close();

            saved = true;
        }
        catch(Exception e)
        {
            e.printStackTrace(System.out);

            saved = false;
        }

        return saved;
    }

    /**
     * Lists the contents of a file
     *
     * @param    fileName the file to list
     */
    private static void listFile(String fileName)
    {
        System.out.println("List file(" + fileName + ")");

        try
        {
            InputStream is = new FileInputStream(fileName);

            int read = is.read();

            while(read != -1)
            {
                System.out.write(read);

                read = is.read();
            }

            is.close();
        }
        catch(Exception e)
        {
            e.printStackTrace();
        }

        System.out.println("List file(" + fileName + ") - done");
    }

    private static Logger LOG = Logger.getLogger(HermesMachineProperties.class.getName());
}
