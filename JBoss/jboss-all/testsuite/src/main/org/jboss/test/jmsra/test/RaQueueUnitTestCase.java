/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmsra.test;
import javax.jms.MessageConsumer;
import javax.jms.Queue;
import javax.jms.QueueConnection;

import javax.jms.QueueConnectionFactory;
import javax.jms.QueueSession;
import javax.jms.Session;

import javax.management.ObjectName;

import javax.naming.Context;

import junit.framework.Test;

import org.jboss.test.JBossTestSetup;

/**
 * Test cases for JMS Resource Adapter use a <em>Queue</em> . <p>
 *
 * Created: Mon Apr 23 21:35:25 2001
 *
 * @author    <a href="mailto:peter.antman@tim.se">Peter Antman</a>
 * @author    <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version   $Revision: 1.3.4.1 $
 */
public class RaQueueUnitTestCase
       extends RaTest
{
   private final static String QUEUE_FACTORY = "ConnectionFactory";
   private final static String QUEUE = "queue/testQueue";
   private final static String JNDI = "TxPublisher";

   /**
    * Constructor for the RaQueueUnitTestCase object
    *
    * @param name           Description of Parameter
    * @exception Exception  Description of Exception
    */
   public RaQueueUnitTestCase(String name) throws Exception
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
      QueueConnectionFactory factory =
            (QueueConnectionFactory)context.lookup(QUEUE_FACTORY);

      connection = factory.createQueueConnection();

      session = ((QueueConnection)connection).createQueueSession(false, Session.AUTO_ACKNOWLEDGE);

      Queue queue = (Queue)context.lookup(QUEUE);

      consumer = ((QueueSession)session).createReceiver(queue);
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
