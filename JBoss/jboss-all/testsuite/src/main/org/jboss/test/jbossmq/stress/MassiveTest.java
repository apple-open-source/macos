/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.stress;

import javax.jms.Message;
import javax.jms.BytesMessage;
import javax.jms.JMSException;

import junit.framework.TestSuite;
import junit.framework.Assert;

import org.jboss.test.jbossmq.MQBase;
/**
 *
 *
 * @author     <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.1 $
 */

public class MassiveTest extends MQBase{
   static byte[] PERFORMANCE_TEST_DATA_PAYLOAD = new byte[10 * 1024];
   public MassiveTest(String name) {
      super(name);
   }

   /**
    * Should be run with large iteration count!!!!!
    */
   public void runMassiveTest() throws Exception {
       // Clean testarea up
         drainTopic();
         
         int ic = getIterationCount();
      
         // Set up a durable subscriber
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.BytesMessage.class,
                                                              "MASSIVE_NR",
                                                              0,
                                                              ic);
                                               
         TopicWorker sub1 = new TopicWorker(SUBSCRIBER,
                                            TRANS_NONE,
                                            f1);
         Thread t1 = new Thread(sub1);
         t1.start();

         // Publish 
         ByteIntRangeMessageCreator c1 = new ByteIntRangeMessageCreator("MASSIVE_NR",
                                                                0);
         TopicWorker pub1 = new TopicWorker(PUBLISHER,
                                            TRANS_NONE,
                                            c1,
                                            ic);
         pub1.connect();
         pub1.publish();
      
         Assert.assertEquals("Publisher did not publish correct number of messages "+pub1.getMessageHandled(),
                             ic,
                             pub1.getMessageHandled());
         
         log.debug("Sleeping for " + ((ic*10)/60000) + " minutes");
         // let sub1 have some time to handle the messages.
         try{ Thread.sleep(ic*10); }catch(InterruptedException e){}
         log.debug("Woke up");
         Assert.assertEquals("Subscriber did not get correct number of messages "+sub1.getMessageHandled(),
                             ic,
                             sub1.getMessageHandled());
      
         // Take down first sub
         sub1.close();
         t1.stop();
         pub1.close();
   }

   public void runMassivTestFailingSub() throws Exception {
             // Clean testarea up
         drainTopic();
         
         int ic = getIterationCount();
      
         // Set up a subscriber
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.BytesMessage.class,
                                                              "MASSIVE_NR",
                                                              0,
                                                              ic);
                                               
         TopicWorker sub1 = new TopicWorker(SUBSCRIBER,
                                            TRANS_NONE,
                                            f1);
         Thread t1 = new Thread(sub1);
         t1.start();

         // Set up a failing sub
         FailingSubWorker sub2 = new FailingSubWorker();
         sub2.setSubscriberAttrs(SUBSCRIBER,
                               TRANS_NONE,
                               f1);
         Thread tf = new Thread(sub2);
         tf.start();
         
         // Publish 
         ByteIntRangeMessageCreator c1 = new ByteIntRangeMessageCreator("MASSIVE_NR",
                                                                0);
         TopicWorker pub1 = new TopicWorker(PUBLISHER,
                                            TRANS_NONE,
                                            c1,
                                            ic);
         pub1.connect();
         pub1.publish();
      
         Assert.assertEquals("Publisher did not publish correct number of messages "+pub1.getMessageHandled(),
                             ic,
                             pub1.getMessageHandled());
         
         log.debug("Sleeping for " + ((ic*10)/60000) + " minutes");
         // let sub1 have some time to handle the messages.
         try{ Thread.sleep(ic*10); }catch(InterruptedException e){}
         log.debug("Woke up");
         Assert.assertEquals("Subscriber did not get correct number of messages "+sub1.getMessageHandled(),
                             ic,
                             sub1.getMessageHandled());
      
         // Take down 
         sub1.close();
         t1.stop();
         pub1.close();
         sub2.setStoped();
         tf.interrupt();
         tf.stop();
         sub2.close();
   }

   public static junit.framework.Test suite() throws Exception{
      
      TestSuite suite= new TestSuite();
      suite.addTest(new MassiveTest("runMassiveTest"));
      
      //suite.addTest(new DurableSubscriberTest("testBadClient"));
      return suite;
   }
   public static void main(String[] args) {
      try {
         MassiveTest t = new MassiveTest("runMassiveTest");
         t.setUp();
         t.runMassiveTest();
      }catch(Exception ex) {
         System.err.println("Ex: " +ex);
         ex.printStackTrace();
      }
      
   }
   public class ByteIntRangeMessageCreator extends IntRangeMessageCreator{
      int start = 0;
      
      public ByteIntRangeMessageCreator(String property) {
         super(property);
      }
      public ByteIntRangeMessageCreator(String property, int start) {
         super(property);
         this.start = start;
      }

      public Message createMessage(int nr) throws JMSException {
         if (session ==null)
            throw new JMSException("Session not allowed to be null");
         
         BytesMessage msg = session.createBytesMessage();
         msg.writeBytes(PERFORMANCE_TEST_DATA_PAYLOAD);
         msg.setStringProperty(property, String.valueOf(start+nr));
         return msg;
      }
   }

   public class FailingSubWorker extends TopicWorker {
      int check = 0;
      //Only reveice firts message
      public void onMessage(Message msg) {
         check++;
         if (check > 1)
            log.warn("Got called while asleep!! " +check);
         while(!stopRequested) {
            sleep(2000);
         }
      }
   }
} // MassiveTest










