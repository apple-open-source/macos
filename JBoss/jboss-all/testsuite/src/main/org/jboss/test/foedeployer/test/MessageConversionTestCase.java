/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.foedeployer.test;

import java.io.IOException;
import java.net.InetAddress;
import java.rmi.RemoteException;
import java.util.Set;
import javax.ejb.CreateException;
import javax.ejb.Handle;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.rmi.PortableRemoteObject;

import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.Session;
import javax.jms.ObjectMessage;
import javax.jms.Topic;
import javax.jms.TopicConnection;
import javax.jms.TopicConnectionFactory;
import javax.jms.TopicPublisher;
import javax.jms.TopicSession;
import javax.jms.Queue;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueReceiver;
import javax.jms.QueueSession;
import javax.jms.MessageListener;

import junit.extensions.TestSetup;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSetup;

import org.jboss.test.foedeployer.ejb.message.QuoteMessage;
import org.apache.log4j.Category;


/**
 * Test of a message driven bean WebLogic Application Conversion
 *
 * @author <a href="mailto:loubyansky@hotmail.com>Alex Loubyansky</a>
 * @version $Revision: 1.2.2.2 $
 *
 * Note: example that comes with WebLogic 6.1 has the following not compliant
 *       with JBoss issues:
 * - WL's client didn't close session and connection after finishing
 *   its publishing.
 *   JBoss produced "SocketException: connection reset by peer..." for this,
 *   which seems like a right behaviour.
 */
public class MessageConversionTestCase
   extends JBossTestCase
{
   // Constants -----------------------------------------------------
   public static final String FOE_DEPLOYER = "foe-deployer-3.2.sar";
   public static final String FOE_DEPLOYER_NAME = "jboss:service=FoeDeployer";
   public static final String CONVERTOR_DEPLOYER_QUERY_NAME = "jboss:service=Convertor,*";
   public static final String MESSAGE_APPLICATION = "foe-deployer-message-test";
   public static final String TOPIC = "topic/testTopic";
   public static final String TOPIC_FACTORY = "ConnectionFactory";
   public static final String QUEUE = "queue/testQueue";
   public static final String QUEUE_FACTORY = "ConnectionFactory";

   public static final int MESSAGES_NUMBER = 10;
   public static final int WAIT_ITERATIONS = 3;

   // Attributes ----------------------------------------------------
   private static TopicConnection topicConnection;
   private static QueueConnection queueConnection;
   private static QueueSession queueSession;
   private static Queue queue;
   private static TopicSession topicSession;
   private static Topic topic;
   private static TopicPublisher topicPublisher;
   private static QuoteMessageListener listener;

   // Static --------------------------------------------------------
   /**
    * Setup the test suite.
    */
   public static Test suite() throws Exception
   {
      TestSuite lSuite = new TestSuite();
      lSuite.addTest(new TestSuite(MessageConversionTestCase.class));

      // Create an initializer for the test suite
      TestSetup lWrapper = new JBossTestSetup(lSuite)
      {
         protected void setUp() throws Exception
         {
            super.setUp();
         }

         protected void tearDown() throws Exception
         {
            super.tearDown();
         }
      };
      return lWrapper;
   }

   // Constructors --------------------------------------------------
   public MessageConversionTestCase(String pName)
   {
      super(pName);
   }

   // Public --------------------------------------------------------
   /**
    * Test an MDB conversion
    */
   public void testMessageConversion()
      throws Exception
   {
      try
      {
         log.debug("+++ testMessageConversion");

         // First check if foe-deployer is deployed
         boolean lIsInitiallyDeployed = getServer().isRegistered(new ObjectName(FOE_DEPLOYER_NAME));
         if(!lIsInitiallyDeployed)
            deploy(FOE_DEPLOYER);

         boolean lIsDeployed = getServer().isRegistered(new ObjectName(FOE_DEPLOYER_NAME));
         assertTrue("Foe-Deployer is not deployed", lIsDeployed);

         // Count number of converters (must be a list one)
         int lCount = getServer().queryNames(new ObjectName(CONVERTOR_DEPLOYER_QUERY_NAME), null).size();
         assertTrue("No Converter found on web server", lCount > 0);

         // Deploy WL application
         deploy(MESSAGE_APPLICATION + ".wlar");

         // Because the Foe-Deployer copies the converted JAR back to the original place
         // it has to be deployed from here again
         deploy(MESSAGE_APPLICATION + ".jar");

         log.debug("getting intial naming context");
         InitialContext ic = new InitialContext();

         //
         // initialize queue stuff
         //
         log.debug("looking for queue connection factory");
         QueueConnectionFactory qcf = (QueueConnectionFactory)ic.lookup( QUEUE_FACTORY );

         log.debug("creating queue connection");
         queueConnection = qcf.createQueueConnection();

         log.debug("creating queue session");
         queueSession = queueConnection.
            createQueueSession( false, Session.AUTO_ACKNOWLEDGE );

         log.debug("looking for queue");
         queue = ( Queue ) ic.lookup( QUEUE );

         log.debug( "creating queue receiver" );
         QueueReceiver receiver = queueSession.createReceiver( queue );

         log.debug( "creating message listener" );
         listener = new QuoteMessageListener();

         log.debug( "registering listener with receiver" );
         receiver.setMessageListener( listener );

         log.debug( "starting queue connection" );
         queueConnection.start();

         //
         // Prepare topic stuff
         //
         log.debug("looking for topic connection factory");
         TopicConnectionFactory cf = (TopicConnectionFactory)ic.lookup(TOPIC_FACTORY);

         log.debug("creating topic connection");
         topicConnection = cf.createTopicConnection();

         log.debug("creating topic session");
         topicSession = topicConnection.
            createTopicSession( false, Session.AUTO_ACKNOWLEDGE );

         log.debug("looking for topic");
         topic = (Topic)ic.lookup( TOPIC );

         log.debug("creating topic publisher");
         topicPublisher = topicSession.createPublisher(topic);

         log.debug("starting topic connection");
         topicConnection.start();

         log.debug("testMessageConversion: sending " +
            MESSAGES_NUMBER + " messages");

         for(int i = 0; i < MESSAGES_NUMBER; ++i)
         {
            QuoteMessage quoteMsg = new QuoteMessage("Topic message no." + i);

            log.debug("publishing message: " + quoteMsg.getQuote() );

            ObjectMessage message = topicSession.createObjectMessage();
            message.setObject(quoteMsg);
            topicPublisher.publish(message);
         }

         log.debug( "waiting for messages to be processed" );
         int i = 0;
         while( (i++ < WAIT_ITERATIONS)
            && (listener.getCount() < MESSAGES_NUMBER) )
         {
            try
            {
               Thread.currentThread().sleep(1000);
            }
            catch(Exception e) {}
         }

         log.debug("Messages received: " + listener.getCount());
         assertTrue("Number of sent messages ("
            + MESSAGES_NUMBER
            + ") isn't equal to number of received ("
            + listener.getCount() + ")",
            MESSAGES_NUMBER == listener.getCount());

         // close connections
         if(topicConnection != null) topicConnection.close();
         if(queueConnection != null) queueConnection.close();

         // undeploy converted application to clean up
         undeploy(MESSAGE_APPLICATION + ".jar" );

         // undeploy WL application
         undeploy(MESSAGE_APPLICATION + ".wlar" );

         // Only undeploy if deployed here
         if(!lIsInitiallyDeployed)
         {
            undeploy(FOE_DEPLOYER);
         }
      }
      catch( Exception e )
      {
         e.printStackTrace();
         throw e;
      }
   }

   // Inner classes -------------------------------------------------
   public class QuoteMessageListener
      implements MessageListener
   {
      // Attributes -------------------------------------------------
      Category log = Category.getInstance( QuoteMessageListener.class );
      public int count = 0;

      // Constructor ------------------------------------------------
      public QuoteMessageListener()
      {
         log.debug( "created" );
         count = 0;
      }

      // Public methods ---------------------------------------------
      public int getCount()
      {
         return count;
      }

      // MessageListener implementation -----------------------------
      public void onMessage(Message msg)
      {
         QuoteMessage quoteMsg = null;
         try
         {
            quoteMsg = (QuoteMessage)((ObjectMessage)msg).getObject();
         }
         catch(ClassCastException cce)
         {
            log.error("Received message isn't of type QuoteMessage: ", cce);
         }
         catch(JMSException jmse)
         {
            log.error("Couldn't fetch message: ", jmse);
         }

         log.debug("received message: " + quoteMsg.getQuote() );
         ++count;
      }
   }
}
