/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.stress;

import javax.jms.*;

import javax.naming.*;

import org.jboss.test.JBossTestCase;
import junit.framework.TestSuite;
import org.apache.log4j.Category;
/**
 * Test failing clients.
 *
 *
 * @author     <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.1 $
 */

public class ClientFailTest extends JBossTestCase{
   final Category log = getLog();
   
   // Provider specific
   static String TOPIC_FACTORY = "ConnectionFactory";
   static String QUEUE_FACTORY = "ConnectionFactory";

   static String TEST_QUEUE = "queue/testQueue";
   static String TEST_TOPIC = "topic/testTopic";

   static Context context;
   //static QueueConnection queueConnection;
   static TopicConnectionFactory topicFactory;

   public ClientFailTest(String name) throws Exception {
      super(name);
   }

   Thread getTopicRunner() {
      Thread sendThread =
         new Thread()
         {
            /**
             * Main processing method for the JBossMQPerfStressTestCase object
             */
            public void run()
            {
               try
               {
                  TopicConnection topicConnection = topicFactory.createTopicConnection();
                  TopicSession session = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
                  Topic topic = (Topic)context.lookup(TEST_TOPIC);

                  TopicPublisher publisher = session.createPublisher(topic);

                  while(true) {
                     try {
                        Thread.sleep(1000);
                     }catch(InterruptedException ex) {

                     }
                  }
                  /*
                  waitForSynchMessage();

                  BytesMessage message = session.createBytesMessage();
                  message.writeBytes(PERFORMANCE_TEST_DATA_PAYLOAD);

                  long startTime = System.currentTimeMillis();
                  for (int i = 0; i < iterationCount; i++)
                  {
                     publisher.publish(topic, message, persistence, 4, 1000L);
                     getLog().debug("  Sent #"+i);
                     if (transacted == TRANS_INDIVIDUAL)
                     {
                        session.commit();
                     }
                  }

                  if (transacted == TRANS_TOTAL)
                  {
                     session.commit();
                  }

                  long endTime = System.currentTimeMillis();
                  session.close();

                  long pTime = endTime - startTime;
                  log.debug("  sent all messages in " + ((double)pTime / 1000) + " seconds. ");
                  */
               }
               catch (Exception e)
               {
                  log.error("error", e);
               }
            }
         };
      return sendThread;
   }

   public void testFailStopTopicPub() throws Exception {
      final int iterationCount = getIterationCount();
      for (int i = 0; i<iterationCount;i++) {
         Thread t = getTopicRunner();
         log.debug("Starting thread " + t.getName());
         t.start();
         log.debug("Sleeping a while");
         Thread.sleep(1500);
         log.debug("Stopping thread " + t.getName());
         t.stop();
      }
      //System.exit(0);
   }

   /**
    * The JUnit setup method
    *
    * @exception Exception  Description of Exception
    */
   protected void setUp() throws Exception
   {

      if (context == null)
      {

         context = getInitialContext();
         /*
         QueueConnectionFactory queueFactory = (QueueConnectionFactory)context.lookup(QUEUE_FACTORY);
         queueConnection = queueFactory.createQueueConnection();
         */
         topicFactory = (TopicConnectionFactory)context.lookup(TOPIC_FACTORY);
         //topicConnection = topicFactory.createTopicConnection();

         getLog().debug("Connection to JBossMQ established.");
      }

   }

   public static junit.framework.Test suite() throws Exception{
      
      TestSuite suite= new TestSuite();
      suite.addTest(new ClientFailTest("testFailStopTopicPub"));
      
      return suite;
   }
   public static void main(String[] args) {
      try {

         ClientFailTest ct = new ClientFailTest("testFailConnect");
         ct.setUp();
         Thread t = ct.getTopicRunner();
         t.run();
         ct.log.debug("Exiting");
         //Thread.sleep(1000);
      }catch(Exception ex) {
         System.out.println("Ex: " +ex);
      }
   }
   
} // ClientFailTest
