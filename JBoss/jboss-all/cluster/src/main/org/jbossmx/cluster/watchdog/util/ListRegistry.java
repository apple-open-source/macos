/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util;

// Standard Java Packages
import java.rmi.*;
import java.rmi.registry.*;

/**
 * Class for listing the contents of a series of RMI Registries
 *
 * @author Stacy Curl
 */
public class ListRegistry
{
    /**
     * Lists the contents of a series of RMI Registries
     *
     * @param    args the names of the RMI registries to list, eg rmi://MachineName
     */
    public static void main(String[] args)
    {
        if(System.getSecurityManager() == null)
        {
            System.setSecurityManager(new RMISecurityManager());
        }

        if(args.length == 0)
        {
            System.out.println("ListRegistry <rmiRegistryUrl>*");
        }
        else
        {
            for(int aLoop = 0; aLoop < args.length; ++aLoop)
            {
                listRegistry(args[aLoop]);
            }
        }
    }

    /**
     * Static method for listing the contents of an RMI Registry
     *
     * @param    rmiRegistryUrl the RMI Registry to list
     */
    private static void listRegistry(final String rmiRegistryUrl)
    {
        try
        {
            System.out.println("ListRegistry: " + rmiRegistryUrl);

            String[] registryContents = Naming.list(rmiRegistryUrl);

            for(int rLoop = 0; rLoop < registryContents.length; ++rLoop)
            {
                displayNameValue(registryContents[rLoop]);
            }

            System.out.println("");
        }
        catch(Exception e)
        {
            e.printStackTrace(System.out);
        }
    }

    /**
     * Display the name and value of a object bound in RMI
     *
     * @param    rmiName the RMI Binding of the object to display
     */
    private static void displayNameValue(final String rmiName)
    {
        try
        {
            final Object value = Naming.lookup(rmiName);

            System.out.println(rmiName + " = " + value);
        }
        catch(Exception e)
        {
            e.printStackTrace(System.out);
        }
    }
}
