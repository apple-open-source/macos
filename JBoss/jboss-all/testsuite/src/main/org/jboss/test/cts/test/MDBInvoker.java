package org.jboss.test.cts.test;

import javax.jms.QueueSession;
import javax.jms.Queue;
import javax.jms.QueueSender;
import javax.jms.TextMessage;
import javax.jms.QueueReceiver;
import javax.jms.Message;
import javax.jms.ObjectMessage;

import EDU.oswego.cs.dl.util.concurrent.CountDown;
import org.apache.log4j.Category;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class MDBInvoker extends Thread
{
   Category log;
   QueueSession session;
   Queue queueA;
   Queue queueB;
   int id;
   CountDown done;
   Exception runEx;

   public MDBInvoker(QueueSession session, Queue queueA, Queue queueB, int id,
         CountDown done, Category log)
   {
      super("MDBInvoker#"+id);
      this.session = session;
      this.queueA = queueA;
      this.queueB = queueB;
      this.id = id;
      this.done = done;
      this.log = log;
   }
   public void run()
   {
      System.out.println("Begin run, this="+this);
      try
      {
         QueueSender sender = session.createSender(queueA);
         TextMessage message = session.createTextMessage();
         message.setText(this.toString());
         sender.send(message);
         QueueReceiver receiver = session.createReceiver(queueB);
         Message reply = receiver.receive(10000);
         if( reply == null )
            runEx = new IllegalStateException("Message receive timeout");
         else if( reply instanceof ObjectMessage )
         {
            ObjectMessage om = (ObjectMessage) reply;
            runEx = (Exception) om.getObject();
         }
         sender.close();
         receiver.close();
      }
      catch(Exception e)
      {
         runEx = e;
      }
      done.release();
      System.out.println("End run, this="+this);
   }

}
