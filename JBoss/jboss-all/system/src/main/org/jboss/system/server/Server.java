/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system.server;

import java.util.Properties;

/**
 * The interface of the basic JBoss server component.
 *
 * @version <tt>$Revision: 1.2.4.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface Server
{
   /** The JMX notification event type sent on end of server startup */
   public final String START_NOTIFICATION_TYPE = "org.jboss.system.server.started";
   /** The JMX notification event type sent on begin of the server shutdown */
   public final String STOP_NOTIFICATION_TYPE = "org.jboss.system.server.stopped";

   /**
    * Initialize the Server instance.
    *
    * @param props     The configuration properties for the server.
    * @return          Typed server configuration object.
    *
    * @throws IllegalStateException    Already initialized.
    * @throws Exception                Failed to initialize.
    */
   void init(Properties props) throws IllegalStateException, Exception;

   /**
    * Get the typed server configuration object which the
    * server has been initalized to use.
    *
    * @return          Typed server configuration object.
    *
    * @throws IllegalStateException    Not initialized.
    */
   ServerConfig getConfig() throws IllegalStateException;

   /**
    * Start the Server instance.
    *
    * @throws IllegalStateException    Already started or not initialized.
    * @throws Exception                Failed to start.
    */
   void start() throws IllegalStateException, Exception;

   /**
    * Check if the server is started.
    *
    * @return   True if the server is started, else false.
    */
   boolean isStarted();

   /**
    * Shutdown the Server instance and run shutdown hooks.  
    *
    * <p>If the exit on shutdown flag is true, then {@link #exit} 
    *    is called, else only the shutdown hook is run.
    *
    * @throws IllegalStateException    No started.
    */
   void shutdown() throws IllegalStateException;

   /**
    * Shutdown the server, the JVM and run shutdown hooks.
    *
    * @param exitcode   The exit code returned to the operating system.
    */
   void exit(int exitcode);

   /** 
    * Forcibly terminates the currently running Java virtual machine.
    *
    * @param exitcode   The exit code returned to the operating system.
    */
   void halt(int exitcode);
}
