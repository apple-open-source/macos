/*
* JBoss, the OpenSource J2EE webOS
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.jmx.connector.rmi;

import java.security.AccessController;
import java.security.PrivilegedAction;

import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;
import javax.management.ReflectionException;
import javax.management.RuntimeErrorException;
import javax.management.RuntimeMBeanException;
import javax.management.RuntimeOperationsException;
import javax.naming.InitialContext; 

/**
* Test Program for the JMX Connector over RMI for the server-side.
*
* It creates a local MBeanServer, loads the RMI Connector and registered
* it as a MBean. At the end it will bind it to the local JNDI server
* (us your own or download the ??.jar and ??.properties from jBoss).
* Afterwards you can download connector.jar from jBoss and test the
* connection.
*
*   @see <related>
*   @author Andreas Schaefer (andreas.schaefer@madplanet.com)
**/
public class TestServer {
	// Constants -----------------------------------------------------> 
	// Attributes ----------------------------------------------------> 
	// Static --------------------------------------------------------
	public static void main(String[] args)
		throws Exception
	{
		// Start server - Main does not have the proper permissions
		AccessController.doPrivileged(
			new PrivilegedAction() {
				public Object run() {
					new TestServer();
					return null;
				}
			}
		);
	}
	
	public TestServer() {
		try {
			System.out.println( "Start local MBeanServer" );
			MBeanServer lServer = MBeanServerFactory.createMBeanServer();

/*
			System.out.println( "Load and register the Logger Service" );
			ObjectName lLoggerName = new ObjectName( lServer.getDefaultDomain(), "service", "Log" );
			lLoggerName = lServer.createMBean(
				"org.jboss.logging.Logger",
				lLoggerName
			).getObjectName();
         
			System.out.println( "Load and register the Log4J Service" );
			ObjectName lLog4JName = new ObjectName( lServer.getDefaultDomain(), "service", "Log4J" );
			lLog4JName = lServer.createMBean(
				"org.jboss.logging.Log4jService",
				lLog4JName
			).getObjectName();
			System.out.println( "Init and Start the Log4J Service" );
//			lServer.invoke( lLog4JName, "init", new Object[] {}, new String[] {} );
			lServer.invoke( lLog4JName, "start", new Object[] {}, new String[] {} );
*/

			System.out.println( "Load and register the Naming Service" );
			ObjectName lNamingName = new ObjectName( lServer.getDefaultDomain(), "service", "naming" );
			lServer.createMBean(
				"org.jboss.naming.NamingService",
				lNamingName
			);
			System.out.println( "Start the Naming Service" );
			try {
			lServer.invoke( lNamingName, "create", new Object[] {}, new String[] {} );
			}
			catch( MBeanException me ) {
				System.err.println( "TestServer.main(), caught: " + me +
					", target: " + me.getTargetException() );
				me.printStackTrace();
				me.getTargetException().printStackTrace();
			}
			catch( Exception e ) {
				e.printStackTrace();
			}
			lServer.invoke( lNamingName, "start", new Object[] {}, new String[] {} );
			System.out.println( "Load and register the JMX RMI-Adaptor" );
			ObjectName lConnectorName = new ObjectName( lServer.getDefaultDomain(), "service", "RMIAdaptor" );
			lServer.createMBean(
				"org.jboss.jmx.connector.rmi.RMIAdaptorService",
				lConnectorName
			);
			System.out.println( "Start the Connector" );
			lServer.invoke( lConnectorName, "create", new Object[] {}, new String[] {} );
			lServer.invoke( lConnectorName, "start", new Object[] {}, new String[] {} );
			System.out.println( "Now open a new Terminal or Command Prompt and start the connector.jar test client" );
		}
		catch( RuntimeMBeanException rme ) {
			System.err.println( "TestServer.main(), caught: " + rme +
				", target: " + rme.getTargetException() );
			rme.printStackTrace();
		}
		catch( MBeanException me ) {
			System.err.println( "TestServer.main(), caught: " + me +
				", target: " + me.getTargetException() );
			me.printStackTrace();
		}
		catch( RuntimeErrorException rte ) {
			System.err.println( "TestServer.main(), caught: " + rte +
				", target: " + rte.getTargetError() );
			rte.printStackTrace();
		}
		catch( ReflectionException re ) {
			System.err.println( "TestServer.main(), caught: " + re +
				", target: " + re.getTargetException() );
			re.printStackTrace();
		}
		catch( Exception e ) {
			System.err.println( "TestServer.main(), caught: " + e );
			e.printStackTrace();
		}
	}
	
	// Constructors --------------------------------------------------> 
	// Public --------------------------------------------------------
}

