/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmsra.test;
import javax.jms.Connection;
import javax.jms.Message;

import javax.jms.MessageConsumer;
import javax.jms.Session;
import javax.naming.Context;

import javax.naming.InitialContext;
import junit.framework.Assert;

import junit.framework.TestCase;

import org.jboss.test.JBossTestCase;

import org.jboss.test.jmsra.bean.*;

/**
 * Abstract test cases for JMS Resource Adapter. <p>
 *
 * Created: Mon Apr 23 21:35:25 2001
 *
 * @author    <a href="mailto:peter.antman@tim.se">Peter Antman</a>
 * @author    <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version   $Revision: 1.9 $
 */
public abstract class RaTest
       extends JBossTestCase
{
   /**
    * Description of the Field
    */
   public final static long DEFAULT_TIMEOUT = 500L;
   /**
    * Description of the Field
    */
   public final static long FLUSH_TIMEOUT = 500L;

   /**
    * Description of the Field
    */
   protected String beanJNDI;
   /**
    * Description of the Field
    */
   protected MessageConsumer consumer;
   /**
    * Description of the Field
    */
   protected Publisher publisher;
   /**
    * Description of the Field
    */
   protected Connection connection;
   /**
    * Description of the Field
    */
   protected Session session;

   /**
    * Constructor for the RaTest object
    *
    * @param name           Description of Parameter
    * @param beanJNDI       Description of Parameter
    * @exception Exception  Description of Exception
    */
   protected RaTest(final String name, final String beanJNDI)
          throws Exception
   {
      super(name);
      this.beanJNDI = beanJNDI;
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testSimple() throws Exception
   {
      printHeader();
      getLog().debug("Verify simple send of message");
      publisher.simple(1);

      Assert.assertEquals(1, getJmsMessage());
      printOK();
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testSimpleFail() throws Exception
   {
      printHeader();
      getLog().debug("Verify simple failed transaction");
      publisher.simpleFail(2);

      Assert.assertEquals(-1, getJmsMessage());
      printOK();
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testBeanOk() throws Exception
   {
      printHeader();
      getLog().debug("Verify bean ok");
      publisher.beanOk(3);

      Assert.assertEquals(3, getJmsMessage());
      printOK();
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testBeanError() throws Exception
   {
      printHeader();
      getLog().debug("Verify bean eroor failed transaction");

      try
      {
         publisher.beanError(4);
      }
      catch (Exception ignore)
      {
      }

      Assert.assertEquals(-1, getJmsMessage());
      printOK();
   }

   /**
    * The JUnit setup method
    *
    * @exception Exception  Description of Exception
    */
   protected void setUp() throws Exception
   {
      // Create a publisher
      Context context = getInitialContext();
      try
      {
         PublisherHome home = (PublisherHome)context.lookup(beanJNDI);
         publisher = home.create();

         init(context);
      }
      finally
      {
         context.close();
      }

      // start up the session
      connection.start();

      // flush the destination
      flush();
   }

   /**
    * Check if we got a message.
    *
    * @return               Publisher.JMS_MESSAGE_NR int property or -1 if no
    *      message was received.
    * @exception Exception  Description of Exception
    */
   protected int getJmsMessage() throws Exception
   {
      return getJmsMessage(DEFAULT_TIMEOUT);
   }

   /**
    * Check if we got a message.
    *
    * @param timeout        The time to wait for a message.
    * @return               Publisher.JMS_MESSAGE_NR int property or -1 if no
    *      message was received.
    * @exception Exception  Description of Exception
    */
   protected int getJmsMessage(long timeout) throws Exception
   {
      Message msg = consumer.receive(timeout);
      if (msg != null)
      {
         getLog().debug("Recived message: " + msg);
         int nr = msg.getIntProperty(Publisher.JMS_MESSAGE_NR);
         getLog().debug("nr: " + nr);
         return nr;
      }
      else
      {
         getLog().debug("NO message recived");
         return -1;
      }
   }

   /**
    * #Description of the Method
    *
    * @param context        Description of Parameter
    * @exception Exception  Description of Exception
    */
   protected abstract void init(final Context context) throws Exception;

   /**
    * The teardown method for JUnit
    *
    * @exception Exception  Description of Exception
    */
   protected void tearDown() throws Exception
   {
      if (consumer != null)
      {
         consumer.close();
      }
      if (connection != null)
      {
         connection.close();
      }
   }

   /**
    * #Description of the Method
    */
   protected void printHeader()
   {
      getLog().debug("\n---- Testing method " + getName() +
            " for bean " + beanJNDI);
   }

   /**
    * #Description of the Method
    */
   protected void printOK()
   {
      getLog().debug("---- Test OK\n");
   }

   /**
    * Flush the destiniation so we know that it contains no messages which might
    * mess up the test.
    *
    * @exception Exception  Description of Exception
    */
   protected void flush() throws Exception
   {
      // getLog().debug(" > Flushing Destination");

      int nr = 0;
      do
      {
         try
         {
            nr = getJmsMessage(FLUSH_TIMEOUT);
         }
         catch (Exception ignore)
         {
         }
      } while (nr != -1);
      // getLog().debug(" > Flushed");
   }


}
