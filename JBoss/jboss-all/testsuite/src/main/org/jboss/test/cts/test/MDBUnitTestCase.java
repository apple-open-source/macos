/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cts.test;


import java.rmi.server.UnicastRemoteObject;
import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueConnection;
import javax.jms.QueueSession;
import javax.jms.Queue;

import junit.framework.Test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.cts.interfaces.StatelessSession;
import org.jboss.test.cts.interfaces.StatelessSessionHome;
import org.jboss.test.cts.interfaces.StrictlyPooledSessionHome;
import EDU.oswego.cs.dl.util.concurrent.CountDown;

/** Basic conformance tests for MDBs
 *
 *  @author Scott.Stark@jboss.org
 *  @version $Revision: 1.1.2.1 $
 */
public class MDBUnitTestCase
   extends JBossTestCase
{
   static final int MAX_SIZE = 20;
   static String QUEUE_FACTORY = "ConnectionFactory";

   public MDBUnitTestCase (String name)
   {
      super(name);
   }

   public void testPooling() throws Exception
   {
      CountDown done = new CountDown(MAX_SIZE);
      InitialContext ctx = new InitialContext();
      QueueConnectionFactory factory = (QueueConnectionFactory) ctx.lookup(QUEUE_FACTORY);
      QueueConnection queConn = factory.createQueueConnection();
      QueueSession session = queConn.createQueueSession(false, QueueSession.AUTO_ACKNOWLEDGE);
      Queue queueA = (Queue) ctx.lookup("queue/A");
      Queue queueB = (Queue) ctx.lookup("queue/B");
      queConn.start();
      MDBInvoker[] threads = new MDBInvoker[MAX_SIZE];
      for(int n = 0; n < MAX_SIZE; n ++)
      {
         MDBInvoker t = new MDBInvoker(session, queueA, queueB, n, done, getLog());
         threads[n] = t;
         t.start();
      }
      super.assertTrue("Acquired done", done.attempt(1500 * MAX_SIZE));
      session.close();
      queConn.close();

      for(int n = 0; n < MAX_SIZE; n ++)
      {
         MDBInvoker t = threads[n];
         if( t.runEx != null )
         {
            t.runEx.printStackTrace();
            super.fail("Inovker.runEx != null");
         }
      }
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(MDBUnitTestCase.class, "cts.jar");
   }

}
