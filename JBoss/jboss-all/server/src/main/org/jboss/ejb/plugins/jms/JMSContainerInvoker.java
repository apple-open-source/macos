/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.ejb.plugins.jms;

import java.lang.reflect.Method;
import java.security.Principal;

import java.util.Collection;
import java.util.Hashtable;

import javax.jms.Connection;
import javax.jms.ConnectionConsumer;
import javax.jms.Destination;
import javax.jms.ExceptionListener;
import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.MessageListener;
import javax.jms.Queue;
import javax.jms.QueueConnection;
import javax.jms.ServerSessionPool;
import javax.jms.Topic;
import javax.jms.TopicConnection;

import javax.management.MBeanServer;
import javax.management.ObjectName;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.naming.NamingException;

import javax.transaction.Transaction;
import javax.transaction.TransactionManager;

import org.w3c.dom.Element;
import org.w3c.dom.Node;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.logging.Logger;
import org.jboss.deployment.DeploymentException;
import org.jboss.util.TCLStack;

import org.jboss.ejb.Container;
import org.jboss.ejb.EJBProxyFactory;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationType;

import org.jboss.jms.ConnectionFactoryHelper;
import org.jboss.jms.asf.ServerSessionPoolFactory;
import org.jboss.jms.asf.StdServerSessionPool;
import org.jboss.jms.jndi.JMSProviderAdapter;

import org.jboss.metadata.MessageDrivenMetaData;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.InvokerProxyBindingMetaData;

/**
 * EJBProxyFactory for JMS MessageDrivenBeans
 *
 * @version <tt>$Revision: 1.50.2.12 $</tt>
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a> .
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 */
public class JMSContainerInvoker
   extends ServiceMBeanSupport
   implements EJBProxyFactory, JMSContainerInvokerMBean
{
   private static final Logger log = Logger.getLogger(JMSContainerInvoker.class);
   
   // Constants -----------------------------------------------------
   
   /**
    * {@link MessageListener#onMessage} reference.
    */
   protected static Method ON_MESSAGE;
   
   /**
    * Default destination type. Used when no message-driven-destination is given
    * in ejb-jar, and a lookup of destinationJNDI from jboss.xml is not
    * successfull. Default value: javax.jms.Topic.
    */
   protected final static String DEFAULT_DESTINATION_TYPE = "javax.jms.Topic";

   /**
    * Initialize the ON_MESSAGE reference.
    */
   static
   {
      try
      {
         final Class type = MessageListener.class;
         final Class arg = Message.class;
         ON_MESSAGE = type.getMethod("onMessage", new Class[]{arg});
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw new ExceptionInInitializerError(e);
      }
   }
   
   // Attributes ----------------------------------------------------
   
   protected boolean optimize;

   /** Maximum number provider is allowed to stuff into a session. */
   protected int maxMessagesNr = 1;

   /** Maximun pool size of server sessions. */
   protected int maxPoolSize = 15;
   
   /** Time to wait before retrying to reconnect a lost connection. */
   protected long reconnectInterval = 10000;
   
   /** If Dead letter queue should be used or not. */
   protected boolean useDLQ = false;
   
   /**
    * JNDI name of the provider adapter.
    * 
    * @see org.jboss.jms.jndi.JMSProviderAdapter
    */
   protected String providerAdapterJNDI;
   
   /**
    * JNDI name of the server session factory.
    * 
    * @see org.jboss.jms.asf.ServerSessionPoolFactory
    */
   protected String serverSessionPoolFactoryJNDI;
   
   /** JMS acknowledge mode, used when session is not XA. */
   protected int acknowledgeMode;

   protected boolean isContainerManagedTx;
   protected boolean isNotSupportedTx;

   /** The container. */
   protected Container container;
   
   /** The JMS connection. */
   protected Connection connection;

   /** The JMS connection consumer. */
   protected ConnectionConsumer connectionConsumer;
   
   protected TransactionManager tm;
   protected ServerSessionPool pool;
   protected ExceptionListenerImpl exListener;

   /** Dead letter queue handler. */
   protected DLQHandler dlqHandler;

   /** DLQConfig element from MDBConfig element from jboss.xml. */
   protected Element dlqConfig;

   protected InvokerProxyBindingMetaData invokerMetaData;
   protected String invokerBinding;

   protected boolean deliveryActive = true;
   
   /**
    * Set the invoker meta data so that the ProxyFactory can initialize properly
    */
   public void setInvokerMetaData(InvokerProxyBindingMetaData imd)
   {
      invokerMetaData = imd;
   }
   
   /**
    * Set the invoker jndi binding
    */
   public void setInvokerBinding(String binding)
   {
      invokerBinding = binding;
   }

   // ContainerService implementation -------------------------------
   
   /**
    * Set the container for which this is an invoker to.
    *
    * @param container  The container for which this is an invoker to.
    */
   public void setContainer(final Container container)
   {
      this.container = container;
   }
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------

   /**
    * @jmx:managed-attribute
    * @return whether delivery is active
    */
   public boolean getDeliveryActive()
   {
      return deliveryActive;
   }

   /**
    * @jmx:managed-operation
    */
   public void startDelivery()
      throws Exception
   {
      if (getState() != STARTED)
         throw new IllegalStateException("The MDB is not started");
      if (deliveryActive)
         return;
      deliveryActive = true;
      startService();
   }

   /**
    * @jmx:managed-operation
    */
   public void stopDelivery()
      throws Exception
   {
      if (getState() != STARTED)
         throw new IllegalStateException("The MDB is not started");
      if (deliveryActive == false)
         return;
      deliveryActive = false;
      stopService();
   }

   /**
    * Sets the Optimized attribute of the JMSContainerInvoker object
    *
    * @param optimize  The new Optimized value
    */
   public void setOptimized(final boolean optimize)
   {
      if (log.isDebugEnabled())
         log.debug("Container Invoker optimize set to " + optimize);
      
      this.optimize = optimize;
   }
   
   // EJBProxyFactory implementation

   /**
    * Always throws an Error
    * 
    * @throws Error Not valid for MDB
    */
   public Object getEJBHome()
   {
      throw new Error("Not valid for MessageDriven beans");
   }

   /**
    * Always throws an Error
    * 
    * @throws Error Not valid for MDB
    */
   public javax.ejb.EJBMetaData getEJBMetaData()
   {
      throw new Error("Not valid for MessageDriven beans");
   }
   
   /**
    * Always throws an Error
    * 
    * @throws Error Not valid for MDB
    */
   public Collection getEntityCollection(Collection ids)
   {
      throw new Error("Not valid for MessageDriven beans");
   }
   
   /**
    * Always throws an Error
    * 
    * @throws Error Not valid for MDB
    */
   public Object getEntityEJBObject(Object id)
   {
      throw new Error("Not valid for MessageDriven beans");
   }
   
   /**
    * Always throws an Error
    * 
    * @throws Error Not valid for MDB
    */
   public Object getStatefulSessionEJBObject(Object id)
   {
      throw new Error("Not valid for MessageDriven beans");
   }
   
   /**
    * Always throws an Error
    * 
    * @throws Error Not valid for MDB
    */
   public Object getStatelessSessionEJBObject()
   {
      throw new Error("Not valid for MessageDriven beans");
   }
   
   /**
    * Gets the Optimized attribute of the JMSContainerInvoker object
    *
    * @return   The Optimized value
    */
   public boolean isOptimized()
   {
      if (log.isDebugEnabled())
         log.debug("Optimize in action: " + optimize);
      
      return optimize;
   }
      
   /**
    * XmlLoadable implementation.
    *
    * FIXME - we ought to move all config into MDBConfig, but I do not
    * do that now due to backward compatibility.
    *
    * @param element                  Description of Parameter
    * @exception DeploymentException  Description of Exception
    */
   public void importXml(final Element element) throws Exception
   {
      try {
         String maxMessages = MetaData.getElementContent
            (MetaData.getUniqueChild(element, "MaxMessages"));
         maxMessagesNr = Integer.parseInt(maxMessages);
      }
      catch (Exception ignore) {}
      
      try {
         String maxSize = MetaData.getElementContent
            (MetaData.getUniqueChild(element, "MaximumSize"));
         maxPoolSize = Integer.parseInt(maxSize);
      }
      catch (Exception ignore) {}
         
      Element mdbConfig = MetaData.getUniqueChild(element, "MDBConfig");
         
      try {
         //
         // jason: should just make this millis and let users convert as needed
         //
         
         String reconnect = MetaData.getElementContent
            (MetaData.getUniqueChild(mdbConfig, "ReconnectIntervalSec"));
         reconnectInterval = Long.parseLong(reconnect)*1000;
      }
      catch (Exception ignore) {}

      try
      {
         if ("false".equalsIgnoreCase(MetaData.getElementContent(
            MetaData.getUniqueChild(mdbConfig, "DeliveryActive"))))
         {
            deliveryActive = false;
         }
      }
      catch (Exception ignore)
      {
      }
         
      // Get Dead letter queue config - and save it for later use
      Element dlqEl = MetaData.getOptionalChild(mdbConfig, "DLQConfig");
      if (dlqEl != null)
      {
         dlqConfig = (Element)((Node)dlqEl).cloneNode(true);
         useDLQ = true;
      }
      else
      {
         useDLQ = false;
      }

      // If these are not found we will get a DeploymentException, I hope
      providerAdapterJNDI = MetaData.getElementContent
         (MetaData.getUniqueChild(element, "JMSProviderAdapterJNDI"));
      
      serverSessionPoolFactoryJNDI = MetaData.getElementContent
         (MetaData.getUniqueChild(element, "ServerSessionPoolFactoryJNDI"));

      //
      // jason: should do away with this, make the config use java:/ explicitly
      // 

      // Check java:/ prefix
      if (!providerAdapterJNDI.startsWith("java:/"))
      {
         providerAdapterJNDI = "java:/" + providerAdapterJNDI;
      }
      
      if (!serverSessionPoolFactoryJNDI.startsWith("java:/"))
      {
         serverSessionPoolFactoryJNDI = "java:/" + serverSessionPoolFactoryJNDI;
      }
   }
   
   /**
    * Initialize the container invoker. Sets up a connection, a server session
    * pool and a connection consumer for the configured destination.
    * 
    * Any JMSExceptions produced while initializing will be assumed to be
    * caused due to JMS Provider failure.
    *
    * @throws Exception  Failed to initalize.
    */
   protected void createService() throws Exception
   {
      importXml(invokerMetaData.getProxyFactoryConfig());

      //
      // jason: this is all whacked out...
      //
      
      exListener = new ExceptionListenerImpl(this);
   }

   /**
    * Initialize the container invoker. Sets up a connection, a server session
    * pool and a connection consumer for the configured destination.
    *
    * @throws Exception  Failed to initalize.
    */
   private void innerCreate() throws Exception
   {
      log.debug("Initializing");

      // Get the JMS provider
      JMSProviderAdapter adapter = getJMSProviderAdapter();
      log.debug("Provider adapter: " + adapter);
      
      // Set up Dead Letter Queue handler  
      if (useDLQ)
      {
         dlqHandler = new DLQHandler(adapter);
         dlqHandler.importXml(dlqConfig);
         dlqHandler.create();
      }
      
      // Store TM reference locally - should we test for CMT Required
      tm = container.getTransactionManager();
      
      // Get configuration information - from EJB-xml
      MessageDrivenMetaData config =
         ((MessageDrivenMetaData)container.getBeanMetaData());
      
      // Selector
      String messageSelector = config.getMessageSelector();
      
      // Queue or Topic - optional unfortunately
      String destinationType = config.getDestinationType();
      
      // Is container managed?
      isContainerManagedTx = config.isContainerManagedTx();
      acknowledgeMode = config.getAcknowledgeMode();
      byte txType = config.getMethodTransactionType("onMessage", 
                                         new Class[]{ Message.class }, 
                                         InvocationType.LOCAL);
      isNotSupportedTx = txType == MetaData.TX_NOT_SUPPORTED; 
      
      // Get configuration data from jboss.xml
      String destinationJNDI = config.getDestinationJndiName();
      String user = config.getUser();
      String password = config.getPasswd();
      
      // Connect to the JNDI server and get a reference to root context
      Context context = adapter.getInitialContext();
      log.debug("context: " + context);
      
      // if we can't get the root context then exit with an exception
      if (context == null)
      {
         throw new RuntimeException("Failed to get the root context");
      }
      
      // Get the JNDI suffix of the destination
      String jndiSuffix = parseJndiSuffix(destinationJNDI, config.getEjbName());
      log.debug("jndiSuffix: " + jndiSuffix);
      
      // Unfortunately the destination is optional, so if we do not have one
      // here we have to look it up if we have a destinationJNDI, else give it
      // a default.
      if (destinationType == null)
      {
         log.warn("No message-driven-destination given; using; guessing type");
         destinationType = getDestinationType(context, destinationJNDI);
      }

      //
      // jason: The following is highly redundant code...
      //
      
      if (destinationType.equals("javax.jms.Topic"))
      {
         log.debug("Got destination type Topic for " + config.getEjbName());
         
         // create a topic connection
         Object factory = context.lookup(adapter.getTopicFactoryRef());
         TopicConnection tConnection = null;
         try
         {
            tConnection = (TopicConnection) ConnectionFactoryHelper.createTopicConnection(factory, user, password);
            connection = tConnection;
         }
         catch (ClassCastException e)
         {
            throw new DeploymentException("Expected a TopicConnection check your provider adaptor: "
               + adapter.getTopicFactoryRef());
         }

         // Fix: ClientId must be set as the first method call after connection creation.
         // Fix: ClientId is necessary for durable subscriptions.

         String clientId = config.getClientId();
         log.debug("Using client id: " + clientId);
         if (clientId != null && clientId.length() > 0)
            connection.setClientID(clientId);

         // lookup or create the destination topic
         Topic topic = null;
         try
         {
            // First we try the specified topic
            if (destinationJNDI != null)
               topic = (Topic) context.lookup(destinationJNDI);
         }
         catch(NamingException e)
         {
            log.warn("Could not find the topic destination-jndi-name=" + destinationJNDI);
         }
         catch(ClassCastException e)
         {
            throw new DeploymentException("Expected a Topic destination-jndi-name=" + destinationJNDI);
         }

         // FIXME: This is not portable, only works for JBossMQ
         if (topic == null)
            topic = (Topic)createDestination(Topic.class,
                                                context,
                                                "topic/" + jndiSuffix,
                                                jndiSuffix);
         
         // set up the server session pool
         pool = createSessionPool(tConnection,
                                  maxPoolSize,
                                  true, // tx
                                  acknowledgeMode ,
                                  new MessageListenerImpl(this));
         
         // To be no-durable or durable
         if (config.getSubscriptionDurability() != MessageDrivenMetaData.DURABLE_SUBSCRIPTION)
         {
            // Create non durable
            connectionConsumer =
               tConnection.createConnectionConsumer(topic,
                                                    messageSelector,
                                                    pool,
                                                    maxMessagesNr);
         }
         else
         {
            // Durable subscription
            String durableName = config.getSubscriptionId();
            
            connectionConsumer =
               tConnection.createDurableConnectionConsumer(topic,
                                                           durableName,
                                                           messageSelector,
                                                           pool,
                                                           maxMessagesNr);
         }
         log.debug("Topic connectionConsumer set up");
      }
      else if (destinationType.equals("javax.jms.Queue"))
      {
         log.debug("Got destination type Queue for " + config.getEjbName());
         
         // create a queue connection
         Object qFactory = context.lookup(adapter.getQueueFactoryRef());
         QueueConnection qConnection = null;
         try
         {
            qConnection = (QueueConnection) ConnectionFactoryHelper.createQueueConnection(qFactory, user, password);
            connection = qConnection;
         }
         catch (ClassCastException e)
         {
            throw new DeploymentException("Expected a QueueConnection check your provider adaptor: "
               + adapter.getQueueFactoryRef());
         }

         // Set the optional client id
         String clientId = config.getClientId();
         log.debug("Using client id: " + clientId);
         if (clientId != null && clientId.length() > 0)
            connection.setClientID(clientId);
         
         // lookup or create the destination queue
         Queue queue = null;
         try
         {
            // First we try the specified queue
            if (destinationJNDI != null)
               queue = (Queue) context.lookup(destinationJNDI);
         }
         catch(NamingException e)
         {
            log.warn("Could not find the queue destination-jndi-name=" + destinationJNDI);
         }
         catch(ClassCastException e)
         {
            throw new DeploymentException("Expected a Queue destination-jndi-name=" + destinationJNDI);
         }

         // FIXME: This is not portable, only works for JBossMQ
         if (queue == null)
            queue = (Queue)createDestination(Queue.class,
                                                context,
                                                "queue/" + jndiSuffix,
                                                jndiSuffix);
         
         // set up the server session pool
         pool = createSessionPool(qConnection,
                                  maxPoolSize,
                                  true, // tx
                                  acknowledgeMode,
                                  new MessageListenerImpl(this));
         log.debug("Server session pool: " + pool);
         
         // create the connection consumer
         connectionConsumer =
            qConnection.createConnectionConsumer(queue,
                                                 messageSelector,
                                                 pool,
                                                 maxMessagesNr);
         log.debug("Connection consumer: " + connectionConsumer);
      }
      
      log.debug("Initialized with config " + toString());

      context.close();
   }
   
   protected void startService() throws Exception
   {
      if (deliveryActive == false)
      {
         log.debug("Delivery is disabled");
         return;
      }
      try
      {
          innerCreate();
      }
      catch (final JMSException e)
      {
      	 //
      	 // start a thread up to handle recovering the connection. so we can
         // attach to the jms resources once they become available
         //
      	 new Thread("JMSContainerInvoker Create Recovery Thread")
         {
            public void run()
            {
               exListener.onException(e);
            }
      	 }.start();
         return;
      }

      if (dlqHandler != null)
      {
         dlqHandler.start();
      }
      
      if (connection != null)
      {
         connection.setExceptionListener(exListener);
         connection.start();
      }
   }
   
   protected void stopService() throws Exception
   {
      // Silence the exception listener
      if (exListener != null)
         exListener.stop();
      
      innerStop();

      // This does nothing - but left in for the future when
      // we support pluggable dlq handlers
      if (dlqHandler != null)
         dlqHandler.stop();
   }

   /**
    * Stop done from inside, we should not stop the
    * exceptionListener in inner stop.
    */
   protected void innerStop()
   {
      try
      {
         if (connection != null)
         {
            connection.setExceptionListener(null);
            log.debug("unset exception listener");
         }
      }
      catch (Exception e)
      {
         log.error("Could not set ExceptionListener to null", e);
      }
      
      // Stop the connection
      try
      {
         if (connection != null)
         {
            connection.stop();
            log.debug("connection stopped");
         }
      }
      catch (Exception e)
      {
         log.error("Could not stop JMS connection", e);
      }

      // close the connection consumer
      try
      {
         if (connectionConsumer != null)
         {
            connectionConsumer.close();
         }
      }
      catch (Exception e)
      {
         log.error("Failed to close connection consumer", e);
      }
      
      // clear the server session pool (if it is clearable)
      try
      {
         if (pool instanceof StdServerSessionPool)
         {
            StdServerSessionPool p = (StdServerSessionPool)pool;
            p.clear();
         }
      }
      catch (Exception e)
      {
         log.error("Failed to clear session pool", e);
      }
      
      // close the connection
      if (connection != null)
      {
         try
         {
            connection.close();
            connection = null;
         }
         catch (Exception e)
         {
            log.error("Failed to close connection", e);
         }
      }

      // Take down DLQ
      try
      {
         if (dlqHandler != null)
         {
            dlqHandler.destroy();
            dlqHandler = null;
         }
      }
      catch (Exception e)
      {
         log.error("Failed to close the dlq handler", e);
      }
   }

   public Object invoke(Object id,
                        Method m,
                        Object[] args,
                        Transaction tx,
                        Principal identity,
                        Object credential)
      throws Exception
   {

      Invocation invocation = new Invocation(id, m, args, tx, identity, credential);
      invocation.setType(InvocationType.LOCAL);
      
      // Set the right context classloader
      TCLStack.push(container.getClassLoader());
      
      try
      {
         return container.invoke(invocation);
      }
      finally
      {
         TCLStack.pop();
      }
   }
   
   /**
    * Try to get a destination type by looking up the destination JNDI, or
    * provide a default if there is not destinationJNDI or if it is not possible
    * to lookup.
    *
    * @param ctx              The naming context to lookup destinations from.
    * @param destinationJNDI  The name to use when looking up destinations.
    * @return                 The destination type, either derived from
    *                         destinationJDNI or DEFAULT_DESTINATION_TYPE
    */
   protected String getDestinationType(Context ctx, String destinationJNDI)
   {
      //
      // jason: this is lame, just return the Destination
      //
      
      String destType = null;
      
      if (destinationJNDI != null)
      {
         try
         {
            Destination dest = (Destination)ctx.lookup(destinationJNDI);
            if (dest instanceof javax.jms.Topic)
            {
               destType = "javax.jms.Topic";
            }
            else if (dest instanceof javax.jms.Queue)
            {
               destType = "javax.jms.Queue";
            }
         }
         catch (NamingException ex)
         {
            log.debug("Could not do heristic lookup of destination ", ex);
         }
         
      }
      if (destType == null)
      {
         //
         // jason: should throw an exception, user should specify this (screw the spec)
         //
         
         log.warn("Could not determine destination type, defaults to: " +
                  DEFAULT_DESTINATION_TYPE);
         
         destType = DEFAULT_DESTINATION_TYPE;
      }
      
      return destType;
   }
   
   /**
    * Return the JMSProviderAdapter that should be used.
    *
    * @return  The JMSProviderAdapter to use.
    */
   protected JMSProviderAdapter getJMSProviderAdapter()
      throws NamingException
   {
      Context context = new InitialContext();
      try
      {
         log.debug("Looking up provider adapter: " + providerAdapterJNDI);
         return (JMSProviderAdapter)context.lookup(providerAdapterJNDI);
      }
      finally
      {
         context.close();
      }
   }
   
   /**
    * Create and or lookup a JMS destination.
    *
    * @param type         Either javax.jms.Queue or javax.jms.Topic.
    * @param ctx          The naming context to lookup destinations from.
    * @param jndiName     The name to use when looking up destinations.
    * @param jndiSuffix   The name to use when creating destinations.
    * @return             The destination.
    * 
    * @throws IllegalArgumentException  Type is not Queue or Topic.
    * @throws Exception                 Description of Exception
    */
   protected Destination createDestination(final Class type,
                                           final Context ctx,
                                           final String jndiName,
                                           final String jndiSuffix)
      throws Exception
   {
      try
      {
         // first try to look it up
         return (Destination)ctx.lookup(jndiName);
      }
      catch (NamingException e)
      {
         // if the lookup failes, the try to create it
         log.warn("destination not found: " + jndiName + " reason: " + e);
         log.warn("creating a new temporary destination: " + jndiName);

         //
         // jason: we should do away with this...
         //
         // attempt to create the destination (note, this is very
         // very, very unportable).
         //
         
         MBeanServer server = org.jboss.mx.util.MBeanServerLocator.locateJBoss();
         
         String methodName;
         if (type == Topic.class)
         {
            methodName = "createTopic";
         }
         else if (type == Queue.class)
         {
            methodName = "createQueue";
         }
         else
         {
            // type was not a Topic or Queue, bad user
            throw new IllegalArgumentException
               ("Expected javax.jms.Queue or javax.jms.Topic: " + type);
         }

         // invoke the server to create the destination
         server.invoke(new ObjectName("jboss.mq:service=DestinationManager"),
                       methodName,
                       new Object[] { jndiSuffix },
                       new String[] { "java.lang.String" });
         
         // try to look it up again
         return (Destination)ctx.lookup(jndiName);
      }
   }
   
   /**
    * Create a server session pool for the given connection.
    *
    * @param connection           The connection to use.
    * @param maxSession           The maximum number of sessions.
    * @param isTransacted         True if the sessions are transacted.
    * @param ack                  The session acknowledgement mode.
    * @param listener             The message listener.
    * @return                     A server session pool.
    * 
    * @throws JMSException
    * @throws NamingException  Description of Exception
    */
   protected ServerSessionPool
      createSessionPool(final Connection connection,
                        final int maxSession,
                        final boolean isTransacted,
                        final int ack,
                        final MessageListener listener)
      throws NamingException, JMSException
   {
      ServerSessionPool pool;
      Context context = new InitialContext();
      
      try
      {
         // first lookup the factory
         log.debug("looking up session pool factory: " +
         serverSessionPoolFactoryJNDI);
         ServerSessionPoolFactory factory = (ServerSessionPoolFactory)
         context.lookup(serverSessionPoolFactoryJNDI);
         
         // the create the pool
         pool = factory.getServerSessionPool
            (connection, maxSession, isTransacted, ack, !isContainerManagedTx || isNotSupportedTx, listener);
      }
      finally
      {
         context.close();
      }
      
      return pool;
   }
   
   /**
    * Parse the JNDI suffix from the given JNDI name.
    *
    * @param jndiname       The JNDI name used to lookup the destination.
    * @param defautSuffix   Description of Parameter
    * @return               The parsed suffix or the defaultSuffix
    */
   protected String parseJndiSuffix(final String jndiname,
      final String defautSuffix)
   {
      // jndiSuffix is merely the name that the user has given the MDB.
      // since the jndi name contains the message type I have to split
      // at the "/" if there is no slash then I use the entire jndi name...
      String jndiSuffix = "";
      
      if (jndiname != null)
      {
         int indexOfSlash = jndiname.indexOf("/");
         if (indexOfSlash != -1)
         {
            jndiSuffix = jndiname.substring(indexOfSlash + 1);
         }
         else
         {
            jndiSuffix = jndiname;
         }
      }
      else
      {
         // if the jndi name from jboss.xml is null then lets use the ejbName
         jndiSuffix = defautSuffix;
      }
      
      return jndiSuffix;
   }
   
   /**
    * An implementation of MessageListener that passes messages on to the
    * container invoker.
    */
   class MessageListenerImpl
      implements MessageListener
   {
      /**
       * The container invoker.
       */
      JMSContainerInvoker invoker;
      // = null;
      
      /**
       * Construct a <tt>MessageListenerImpl</tt> .
       *
       * @param invoker  The container invoker. Must not be null.
       */
      MessageListenerImpl(final JMSContainerInvoker invoker)
      {
         // assert invoker != null;
         
         this.invoker = invoker;
      }
      
      /**
       * Process a message.
       *
       * @param message  The message to process.
       */
      public void onMessage(final Message message)
      {
         // assert message != null;
         if (log.isTraceEnabled())
         {
            log.trace("processing message: " + message);
         }
         
         Object id;
         try
         {
            id = message.getJMSMessageID();
         }
         catch (JMSException e)
         {
            // what ?
            id = "JMSContainerInvoker";
         }

         // Invoke, shuld we catch any Exceptions??
         try
         {
            Transaction tx = tm.getTransaction();

            // DLQHandling
            if (useDLQ && // Is Dead Letter Queue used at all
               message.getJMSRedelivered() && // Was message resent
               dlqHandler.handleRedeliveredMessage(message, tx)) //Did the DLQ handler take care of the message
            {
               // Message will be placed on Dead Letter Queue,
               // if redelivered to many times
               return;
            }
            
            invoker.invoke(id,                    // Object id - where used?
                           ON_MESSAGE,            // Method to invoke
                           new Object[]{message}, // argument
                           tx,                    // Transaction
                           null,                  // Principal                           
                           null);                 // Cred
                           
         }
         catch (Exception e)
         {
            log.error("Exception in JMSCI message listener", e);
         }
      }
   }
   
   /**
    * ExceptionListener for failover handling.
    */
   class ExceptionListenerImpl
      implements ExceptionListener
   {
      JMSContainerInvoker invoker;
      Thread currentThread;
      boolean notStoped = true;
      
      ExceptionListenerImpl(final JMSContainerInvoker invoker)
      {
         this.invoker = invoker;
      }
      
      /**
       * #Description of the Method
       *
       * @param ex  Description of Parameter
       */
      public void onException(JMSException ex)
      {
         currentThread = Thread.currentThread();
         
         log.warn("JMS provider failure detected: ", ex);
         boolean tryIt = true;
         while (tryIt && notStoped)
         {
            log.info("Trying to reconnect to JMS provider");
            try
            {
               try
               {
                  Thread.sleep(reconnectInterval);
               }
               catch (InterruptedException ie)
               {
                  tryIt = false;
                  return;
               }
               
               // Reboot container
               invoker.innerStop();
               invoker.destroyService();
               invoker.startService();
               tryIt = false;
               
               log.info("Reconnected to JMS provider");
            }
            catch (Exception e)
            {
               log.error("Reconnect failed: JMS provider failure detected:", e);
            }
         }
         currentThread = null;
      }

      void stop()
      {
         log.debug("Stop requested");
         
         notStoped = false;
         if (currentThread != null)
         {
            currentThread.interrupt();
            log.debug("Current thread interrupted");
         }
      }
   }
   
   /**
    * Return a string representation of the current config state.
    */
   public String toString()
   {
      return super.toString() + 
         "{ maxMessagesNr=" + maxMessagesNr +
         ", maxPoolSize=" + maxPoolSize +
         ", reconnectInterval=" + reconnectInterval +
         ", providerAdapterJNDI=" + providerAdapterJNDI +
         ", serverSessionPoolFactoryJNDI=" + serverSessionPoolFactoryJNDI +
         ", acknowledgeMode=" + acknowledgeMode +
         ", isContainerManagedTx=" + isContainerManagedTx +
         ", isNotSupportedTx=" + isNotSupportedTx +
         ", useDLQ=" +useDLQ +
         ", dlqHandler=" + dlqHandler +
         " }";
   }   
}
