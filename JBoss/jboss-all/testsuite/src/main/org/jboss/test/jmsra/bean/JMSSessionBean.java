package org.jboss.test.jmsra.bean;

import javax.ejb.*;
import javax.jms.*;
import javax.naming.*;

import org.jboss.test.util.ejb.SessionSupport;

public class JMSSessionBean extends SessionSupport
{
   public void sendToQueueAndTopic()
   {
      try
      {
         InitialContext ctx = new InitialContext();

         QueueConnectionFactory qcf = (QueueConnectionFactory) ctx.lookup("java:/JmsXA");
         Queue q = (Queue) ctx.lookup("queue/testQueue");
         QueueConnection qc = qcf.createQueueConnection();
         QueueSession qs = null;
         try
         {
            qs = qc.createQueueSession(true, Session.AUTO_ACKNOWLEDGE);
            QueueSender sender = qs.createSender(q);
            sender.send(qs.createMessage());
         }
         finally
         {
            if (qs != null)
               qs.close();
            if (qc != null)
               qc.close();
         }

         TopicConnectionFactory tcf = (TopicConnectionFactory) ctx.lookup("java:/JmsXA");
         Topic t = (Topic) ctx.lookup("topic/testTopic");
         TopicConnection tc = tcf.createTopicConnection();
         TopicSession ts = null;
         try
         {
            ts = tc.createTopicSession(true, Session.AUTO_ACKNOWLEDGE);
            TopicPublisher publisher = ts.createPublisher(t);
            publisher.publish(ts.createMessage());
         }
         finally
         {
            if (ts != null)
               ts.close();
            if (tc != null)
               tc.close();
         }
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
   }
}