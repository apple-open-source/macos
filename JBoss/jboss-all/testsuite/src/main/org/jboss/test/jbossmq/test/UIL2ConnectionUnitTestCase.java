/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.test;

import java.util.Properties;

import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.XAQueueConnection;
import javax.jms.XAQueueConnectionFactory;
import javax.naming.InitialContext;

import org.jboss.mq.SpyConnectionFactory;
import org.jboss.mq.SpyXAConnectionFactory;
import org.jboss.test.JBossTestCase;

/** 
 * Test all the verious ways that a connection can be 
 * established with JBossMQ
 *
 * @author hiram.chirino@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class UIL2ConnectionUnitTestCase extends JBossTestCase
{

   public UIL2ConnectionUnitTestCase(String name)
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
   }

   public void testMultipleUIL2ConnectViaJNDI() throws Exception
   {

      getLog().debug("Starting testMultipleUIL2ConnectViaJNDI");

      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("UIL2ConnectionFactory");

      QueueConnection connections[] = new QueueConnection[10];

      getLog().debug("Creating " + connections.length + " connections.");
      for (int i = 0; i < connections.length; i++)
      {
         connections[i] = cf.createQueueConnection();
      }

      getLog().debug("Closing the connections.");
      for (int i = 0; i < connections.length; i++)
      {
         connections[i].close();
      }

      getLog().debug("Finished testMultipleUIL2ConnectViaJNDI");
   }

   public void testUIL2ConnectViaJNDI() throws Exception
   {
      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("UIL2ConnectionFactory");
      QueueConnection c = cf.createQueueConnection();
      c.close();

      XAQueueConnectionFactory xacf = (XAQueueConnectionFactory) ctx.lookup("UIL2XAConnectionFactory");
      XAQueueConnection xac = xacf.createXAQueueConnection();
      xac.close();
   }

   public void testUIL2ConnectNoJNDI() throws Exception
   {

      Properties props = new Properties();
      props.setProperty(
         org.jboss.mq.il.uil2.UILServerILFactory.SERVER_IL_FACTORY_KEY,
         org.jboss.mq.il.uil2.UILServerILFactory.SERVER_IL_FACTORY);
      props.setProperty(
         org.jboss.mq.il.uil2.UILServerILFactory.CLIENT_IL_SERVICE_KEY,
         org.jboss.mq.il.uil2.UILServerILFactory.CLIENT_IL_SERVICE);
      props.setProperty(org.jboss.mq.il.uil2.UILServerILFactory.PING_PERIOD_KEY, "1000");
      props.setProperty(org.jboss.mq.il.uil2.UILServerILFactory.UIL_ADDRESS_KEY, "localhost");
      props.setProperty(org.jboss.mq.il.uil2.UILServerILFactory.UIL_PORT_KEY, "8093");

      QueueConnectionFactory cf = new SpyConnectionFactory(props);
      QueueConnection c = cf.createQueueConnection();
      c.close();

      XAQueueConnectionFactory xacf = new SpyXAConnectionFactory(props);
      XAQueueConnection xac = xacf.createXAQueueConnection();
      xac.close();
   }

   public static void main(java.lang.String[] args)
   {
      junit.textui.TestRunner.run(UIL2ConnectionUnitTestCase.class);
   }
}
