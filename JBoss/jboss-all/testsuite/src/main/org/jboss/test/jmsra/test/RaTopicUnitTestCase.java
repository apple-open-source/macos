/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmsra.test;
import javax.jms.MessageConsumer;
import javax.jms.Session;
import javax.jms.Topic;
import javax.jms.TopicConnection;

import javax.jms.TopicConnectionFactory;
import javax.jms.TopicSession;

import javax.management.ObjectName;

import javax.naming.Context;

import junit.framework.Test;

import org.jboss.test.JBossTestSetup;

/**
 * Test cases for JMS Resource Adapter using a <em>Topic</em> . <p>
 *
 * Created: Mon Apr 23 21:35:25 2001
 *
 * @author    <a href="mailto:peter.antman@tim.se">Peter Antman</a>
 * @author    <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version   $Revision: 1.3.4.1 $
 */
public class RaTopicUnitTestCase
       extends RaTest
{
   private final static String TOPIC_FACTORY = "ConnectionFactory";
   private final static String TOPIC = "topic/testTopic";
   private final static String JNDI = "TxTopicPublisher";

   /**
    * Constructor for the RaTopicUnitTestCase object
    *
    * @param name           Description of Parameter
    * @exception Exception  Description of Exception
    */
   public RaTopicUnitTestCase(String name) throws Exception
   {
      super(name, JNDI);
   }

   /**
    * #Description of the Method
    *
    * @param context        Description of Parameter
    * @exception Exception  Description of Exception
    */
   protected void init(final Context context) throws Exception
   {
      TopicConnectionFactory factory =
            (TopicConnectionFactory)context.lookup(TOPIC_FACTORY);

      connection = factory.createTopicConnection();

      session = ((TopicConnection)connection).createTopicSession(false, Session.AUTO_ACKNOWLEDGE);

      Topic topic = (Topic)context.lookup(TOPIC);

      consumer = ((TopicSession)session).createSubscriber(topic);
   }

   public static Test suite() throws Exception
   {
      return new JBossTestSetup(getDeploySetup(RaQueueUnitTestCase.class, "jmsra.jar"))
         {
             protected void tearDown() throws Exception
             {
                super.tearDown();

                // Remove the messages
                getServer().invoke
                (
                   new ObjectName("jboss.mq.destination:service=Queue,name=testQueue"),
                   "removeAllMessages",
                   new Object[0],
                   new String[0]
                );
             }
          };
   }


}
