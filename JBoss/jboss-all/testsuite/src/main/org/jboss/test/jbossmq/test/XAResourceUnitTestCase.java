/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.test;

import javax.jms.Message;
import javax.jms.Topic;
import javax.jms.TopicPublisher;
import javax.jms.TopicSession;
import javax.jms.XATopicConnection;
import javax.jms.XATopicConnectionFactory;
import javax.jms.XATopicSession;
import javax.naming.InitialContext;
import javax.transaction.xa.XAResource;
import javax.transaction.xa.Xid;

import org.jboss.test.JBossTestCase;

/**
 * Rollback tests
 *
 * @author
 * @version
 */
public class XAResourceUnitTestCase extends JBossTestCase
{
   static String XA_TOPIC_FACTORY = "XAConnectionFactory";

   static String TEST_TOPIC = "topic/testTopic";

   public XAResourceUnitTestCase(String name) throws Exception
   {
      super(name);
   }

   public void testXAResourceSuspendWorkCommit() throws Exception
   {
      InitialContext context = getInitialContext();
      XATopicConnectionFactory factory = (XATopicConnectionFactory) context.lookup(XA_TOPIC_FACTORY);
      Topic topic = (Topic) context.lookup(TEST_TOPIC);

      XATopicConnection connection = (XATopicConnection) factory.createXATopicConnection();
      try
      {
         // Set up
         XATopicSession xaSession = connection.createXATopicSession();
         TopicSession session = xaSession.getTopicSession();
         TopicPublisher publisher = session.createPublisher(topic);
         Message message = session.createTextMessage();

         // Add the xa resource to xid1
         MyXid xid1 = new MyXid();
         XAResource resource = xaSession.getXAResource();
         resource.start(xid1, XAResource.TMNOFLAGS);

         // Do some work
         publisher.publish(message);

         // Suspend the transaction
         resource.end(xid1, XAResource.TMSUSPEND);

         // Add the xa resource to xid2
         MyXid xid2 = new MyXid();
         resource.start(xid2, XAResource.TMNOFLAGS);

         // Do some work in the new transaction
         publisher.publish(message);

         // Commit the first transaction and end the branch
         resource.end(xid1, XAResource.TMSUCCESS);
         resource.commit(xid1, true);

         // Do some more work in the new transaction
         publisher.publish(message);

         // Commit the second transaction and end the branch
         resource.end(xid2, XAResource.TMSUCCESS);
         resource.commit(xid2, true);
      }
      finally
      {
         connection.close();
      }
   }

   public static class MyXid implements Xid
   {
      static byte next = 0;

      byte[] xid;

      public MyXid()
      {
         xid = new byte[] { ++next };
      }

      public int getFormatId()
      {
         return 314;
      }

      public byte[] getGlobalTransactionId()
      {
         return xid;
      }

      public byte[] getBranchQualifier()
      {
         return null;
      }
   }
}
