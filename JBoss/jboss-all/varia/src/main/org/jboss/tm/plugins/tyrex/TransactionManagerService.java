/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.tm.plugins.tyrex;

import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.Hashtable;
import javax.naming.InitialContext;
import javax.naming.Context;
import javax.naming.Reference;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.NameParser;
import javax.naming.spi.ObjectFactory;

import javax.management.MBeanServer;
import javax.management.ObjectName;

import javax.transaction.TransactionManager;
import tyrex.tm.TransactionDomain;
import org.omg.CosTransactions.TransactionFactory;

import org.jboss.system.ServiceMBeanSupport;

import org.apache.log4j.Category;

/**
 * This is a JMX service which manages the Tyrex TransactionManager (tyrex.exolab.org).
 * The service creates it and binds a Reference to it into JNDI. It also initializes
 * the object that manages Tyrex TransactionPropagationContext.
 *
 * @jmx:mbean name="jboss:type=TransactionManager,flavor=Tyrex"
 *            extends="org.jboss.system.ServiceMBean"
 *
 * @see TyrexTransactionPropagationContextManager, tyrex.tm.TransactionDomain
 *
 * @version <tt>$Revision: 1.7 $</tt>
 * @author <a href="mailto:akkerman@cs.nyu.edu">Anatoly Akkerman</a>
 */
public class TransactionManagerService
   extends ServiceMBeanSupport
   implements TransactionManagerServiceMBean, ObjectFactory
{
   // Private ----
   Category log = Category.getInstance(this.getClass());
   // Constants -----------------------------------------------------
   public static String JNDI_NAME = "java:/TransactionManager";
   public static String JNDI_TPC_SENDER = "java:/TPCSender";
   public static String JNDI_TPC_RECEIVER = "java:/TPCReceiver";
   public static String JNDI_IMPORTER = "java:/TransactionPropagationContextImporter";
   public static String JNDI_EXPORTER = "java:/TransactionPropagationContextExporter";

   // Attributes ----------------------------------------------------

   MBeanServer server;

   String configFile = "domain.xml";


   // Static --------------------------------------------------------

   static TransactionDomain txDomain = null;
   static TransactionManager tm = null;
   static TransactionFactory txFactory = null; //implements Sender and Receiver as well
   static TyrexTransactionPropagationContextManager tpcManager = null;

   // ServiceMBeanSupport overrides ---------------------------------

   public String getName()
   {
      return "Tyrex Transaction manager";
   }

   protected ObjectName getObjectName(MBeanServer server, ObjectName name)
      throws javax.management.MalformedObjectNameException
   {
      this.server = server;
      return OBJECT_NAME;
   }

   protected void startService()
      throws Exception
   {
      // Create txDomain singleton if we did not do it yet.
      if (txDomain == null) {
        txDomain = tyrex.tm.TransactionDomain.createDomain( configFile );
        txDomain.recover();
        tm = txDomain.getTransactionManager();
        txFactory = txDomain.getTransactionFactory();
      }

      // Bind reference to TM in JNDI
      // Tyrex TM does not implement the tx importer and exporter
      // interfaces. These are handled through a different class.
      bindRef(JNDI_NAME, "javax.transaction.TransactionManager");
      bindRef(JNDI_TPC_SENDER, "org.omg.CosTSPortability.Sender");
      bindRef(JNDI_TPC_RECEIVER, "org.omg.CosTSPortability.Receiver");

      // This Manager implements the importer and exporter interfaces
      // but relies on the org.omg.CosTSPortability Sender and Receiver
      // to be bound in the JNDI as TPCSender and TPCReceiver
      // so we can initialize it only after we bind these names to JNDI
      if (tpcManager == null) {
        tpcManager = new TyrexTransactionPropagationContextManager();
      }
      bindRef(JNDI_IMPORTER, "org.jboss.tm.TransactionPropagationContextImporter");
      bindRef(JNDI_EXPORTER, "org.jboss.tm.TransactionPropagationContextFactory");
   }

   protected void stopService() throws Exception
   {
      // Remove TM
      Context ctx = new InitialContext();
      try {
         ctx.unbind(JNDI_NAME);
         ctx.unbind(JNDI_TPC_SENDER);
         ctx.unbind(JNDI_TPC_RECEIVER);
         ctx.unbind(JNDI_IMPORTER);
         ctx.unbind(JNDI_EXPORTER);
      } 
      catch (Exception e) {
         ctx.close();
      }
   }

   /**
    * @jmx:managed-attribute
    */
   public String getConfigFileName()
   {
      return this.configFile;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setConfigFileName(String name) throws IOException
   {
      // See if the name is a URL
      try
      {
         URL url = new URL(name);
         configFile = url.toExternalForm();
      }
      catch(MalformedURLException e)
      {
         // Try to resolve the name as a classpath resource
	 /*
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         URL url = loader.getResource(name);
         if( url == null )
         {
            String msg = "Failed to find name: "+name+" as either a URL or a classpath resource";
            log.error(msg);
            throw new IOException(msg);
         }
         configFile = url.toExternalForm();
 	 */
	 configFile = name;
      }
      log.info("Using configFile: "+configFile);
   }

   // ObjectFactory implementation ----------------------------------

   public Object getObjectInstance(Object obj, Name name,
                                   Context nameCtx, Hashtable environment)
      throws Exception
   {
      NameParser parser;
      Name tmName = null;
      Name senderName = null;
      Name receiverName = null;
      Name exporterName = null;
      Name importerName = null;

      if (nameCtx != null) {
        parser = nameCtx.getNameParser(nameCtx.getNameInNamespace());
      } else {
        Context ctx = new InitialContext();
        parser = ctx.getNameParser(ctx.getNameInNamespace());
      }

      try {
        tmName = parser.parse("TransactionManager");
        senderName = parser.parse("TPCSender");
        receiverName = parser.parse("TPCReceiver");
        exporterName = parser.parse("TransactionPropagationContextImporter");
        importerName = parser.parse("TransactionPropagationContextExporter");
      }
      catch (NamingException e) {
        e.printStackTrace();
      }

// DEBUG      log.debug("Obtaining object instance for: " + name);

// DEBUG
   /*
      log.debug("My composite names: " + tmName +
                   ", " + senderName +
                   ", " + receiverName +
                   ", " + exporterName +
                   ", " + importerName);
    */
      if (name.endsWith(tmName)) {
        // Return the transaction manager
        return tm;
      }
      else if (name.endsWith(senderName) ||
               name.endsWith(receiverName)) {
        return txFactory;
      }
      else if (name.endsWith(exporterName) ||
               name.endsWith(importerName)) {
        return tpcManager;
      }
      else {
        log.warn("TransactionManagerService: requested an unknown object:" + name);
        return null;
      }

   }


   // Private -------------------------------------------------------

   private void bindRef(String jndiName, String className)
      throws Exception
   {
      Reference ref = new Reference(className, getClass().getName(), null);
      new InitialContext().bind(jndiName, ref);
   }
}
