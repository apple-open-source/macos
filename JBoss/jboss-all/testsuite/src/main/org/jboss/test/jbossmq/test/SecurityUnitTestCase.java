/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.test;

import javax.management.ObjectName;

import junit.framework.Assert;
import junit.framework.Test;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestSetup;
import org.jboss.test.jbossmq.MQBase;

/**
 * Test of security features in JBossMQ
 *
 *
 * @author     <a href="pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.2.2.4 $
 */
public class SecurityUnitTestCase extends MQBase
{
   
   public SecurityUnitTestCase(String name)
   {
      super(name);
   }
   
   public void runLoginTest() throws Exception
   {
      TopicWorker sub1 = null;
      TopicWorker pub1 = null;
      try
      {
         drainTopic();
         int ic = 5;
         
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
            "USER_NR",
            0,
            ic);
         
         sub1 = new TopicWorker(SUBSCRIBER,
            TRANS_NONE,
            f1);
         sub1.setUser("john", "needle");
         Thread t1 = new Thread(sub1);
         t1.start();
         
         // Publish
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("USER_NR", 0);
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            c1,
            ic);
         pub1.connect();
         pub1.publish();
         
         Assert.assertEquals("Publisher did not publish correct number of messages "+pub1.getMessageHandled(),
            ic,
            pub1.getMessageHandled());
         
         // let sub1 have some time to handle the messages.
         log.debug("Sleeping for " + ((ic*10)/60000) + " minutes");
         sleep(ic*100);
         
         
         Assert.assertEquals("Subscriber did not get correct number of messages "+sub1.getMessageHandled(),
            ic,
            sub1.getMessageHandled());
         sub1.close();
         pub1.close();
      }catch(Throwable t)
      {
         if (t instanceof junit.framework.AssertionFailedError)
            throw (junit.framework.AssertionFailedError)t;
         
         log.error("Error in test: " +t,t);
         throw new Exception(t.getMessage());
      }finally
      {
         try
         {
            if (sub1 != null)
               sub1.close();
         }catch(Exception ex)
         {}
         try
         {
            if (pub1 != null)
               pub1.close();
         }catch(Exception ex)
         {}
      }
      
   }
   
   /**
    Tests that check authentication
    1. Login without cred
    2. Login with valid usedid,pwd
    3. Login with valid user, unvalid pwd
    4. Login with unvalid user.
    */
   public void runLoginNoCred()throws Exception
   {
      TopicWorker pub1 = null;
      try
      {
         drainTopic();
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            null,
            0);
         pub1.connect();
      }catch(Exception ex)
      {
         Assert.fail("Could lot login without any cred");
      } finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {            
         }
      }
   }
   
   public void runLoginValidCred() throws Exception
   {
      TopicWorker pub1 = null;
      try
      {
         drainTopic();
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            null,
            0);
         pub1.setUser("john", "needle");
         pub1.connect();
      }catch(Exception ex)
      {
         Assert.fail("Could lot login with valid cred");
      } finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   public void runLoginInvalidPwd() throws Exception
   {
      TopicWorker pub1 = null;
      try
      {
         drainTopic();
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            null,
            0);
         pub1.setUser("john", "bogus");
         Exception e = null;
         try
         {
            pub1.connect();
         }catch(Exception ex)
         {
            e = ex;
         }
         log.debug(e);
         Assert.assertTrue("Loggin in with invalid password did not throw correct exception", e instanceof javax.jms.JMSSecurityException);
         
      } finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   public void runLoginInvalidCred() throws Exception
   {
      TopicWorker pub1 = null;
      try
      {
         drainTopic();
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            null,
            0);
         pub1.setUser("bogus", "bogus");
         Exception e = null;
         try
         {
            pub1.connect();
         }catch(Exception ex)
         {
            e = ex;
         }
         log.debug(e);
         Assert.assertTrue("Loggin in with invalid user did not throw correct exception", e instanceof javax.jms.JMSSecurityException);
         
      } finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   
   
   /**
    An number of tests to verrify that clientID works as expected:
    
    1. Nothing. getClientID should return a string starting withID
    2. user/pwd with preconfigured clientID, should return preconf
    3. setClientID, should return the set clientID
    4. setClienID starting with ID, should trow invalid clientID
    5. setClientID same as a preconf, should trow invalid clientID
    6. setClientID after any method beeing invoked on con, throw invalid state
    */
   public void runClientIDNormalTest() throws Exception
   {
      TopicWorker pub1 = null;
      try
      {
         drainTopic();
         int ic = 5;
         
         // Publish
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("USER_NR",0);
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            c1,
            ic);
         pub1.connect();
         pub1.publish();
         
         Assert.assertTrue("Client did not get a valid clientID", pub1.connection.getClientID().startsWith("ID"));
         pub1.close();
         
      }catch(Throwable t)
      {
         if (t instanceof junit.framework.AssertionFailedError)
            throw (junit.framework.AssertionFailedError)t;
         log.error("Error in test: " +t,t);
         throw new Exception(t.getMessage());
      }finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   public void runClientIDPreconfTest() throws Exception
   {
      TopicWorker pub1 = null;
      try
      {
         drainTopic();
         int ic = 5;
         
         // Publish
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("USER_NR", 0);
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            c1,
            ic);
         pub1.setUser("john","needle");
         pub1.connect();
         pub1.publish();
         
         Assert.assertEquals("Client did not get a valid clientID", "DurableSubscriberExample",pub1.connection.getClientID());
         pub1.close();
         
      }catch(Throwable t)
      {
         if (t instanceof junit.framework.AssertionFailedError)
            throw (junit.framework.AssertionFailedError)t;
         log.error("Error in test: " +t,t);
         throw new Exception(t.getMessage());
      }finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   public void runClientIDSetTest() throws Exception
   {
      TopicWorker pub1 = null;
      try
      {
         drainTopic();
         int ic = 5;
         
         // Publish
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("USER_NR", 0);
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            c1,
            ic);
         pub1.setClientID("myId");
         pub1.connect();
         pub1.publish();
         
         Assert.assertEquals("Client did not get a valid clientID", "myId",pub1.connection.getClientID());
         pub1.close();
         
      }catch(Throwable t)
      {
         if (t instanceof junit.framework.AssertionFailedError)
            throw (junit.framework.AssertionFailedError)t;
         log.error("Error in test: " +t,t);
         throw new Exception(t.getMessage());
      }finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   public void runClientIDSetInternal() throws Exception
   {
      TopicWorker pub1 = null;
      try
      {
         drainTopic();
         int ic = 5;
         
         // Publish
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("USER_NR", 0);
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            c1,
            ic);
         pub1.setClientID("ID123");
         Exception e = null;
         try
         {
            pub1.connect();
         }catch(Exception ex)
         {
            e = ex;
         }
         log.debug(e);
         Assert.assertTrue("Setting a clientID looking like an internal did not throw correct exception", e instanceof javax.jms.InvalidClientIDException);
         
         pub1.close();
         
      }catch(Throwable t)
      {
         if (t instanceof junit.framework.AssertionFailedError)
            throw (junit.framework.AssertionFailedError)t;
         log.error("Error in test: " +t,t);
         throw new Exception(t.getMessage());
      }finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   public void runClientIDSetSteelPreconf() throws Exception
   {
      TopicWorker pub1 = null;
      try
      {
         drainTopic();
         int ic = 5;
         
         // Publish
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("USER_NR", 0);
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            c1,
            ic);
         pub1.setClientID("DurableSubscriberExample");
         Exception e = null;
         try
         {
            pub1.connect();
         }catch(Exception ex)
         {
            e = ex;
         }
         log.debug(e);
         Assert.assertTrue("Setting a clientID wich is preconfigured did not throw correct exception", e instanceof javax.jms.InvalidClientIDException);
         
         pub1.close();
         
      }catch(Throwable t)
      {
         if (t instanceof junit.framework.AssertionFailedError)
            throw (junit.framework.AssertionFailedError)t;
         log.error("Error in test: " +t,t);
         throw new Exception(t.getMessage());
      }finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   public void runClientIDSetAfterInvoke() throws Exception
   {
      TopicWorker pub1 = null;
      try
      {
         drainTopic();
         int ic = 5;
         
         // Publish
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("USER_NR", 0);
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            c1,
            ic);
         
         pub1.connect();
         pub1.publish();
         Exception e = null;
         try
         {
            pub1.connection.setClientID("myID");
            
         }catch(Exception ex)
         {
            e = ex;
         }
         log.debug(e);
         Assert.assertTrue("Setting a clientID after connection is used did not throw correct exception: " +e,
            e instanceof javax.jms.IllegalStateException);
         
         pub1.close();
         
      }catch(Throwable t)
      {
         if (t instanceof junit.framework.AssertionFailedError)
            throw (junit.framework.AssertionFailedError)t;
         log.error("Error in test: " +t,t);
         throw new Exception(t.getMessage());
      }finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   
   /**
    Tests to check autorization.
    
    Remember there are actuallt two types of fails:
    a) You are a user, but do no belong to a group that has acl.
    b) You belong to a group, but that group does not have acl.
    we test the first for topics and the second for queues, by
    configuration in jbossmq-testsuite-service.xml
    
    Tests that check autorization.
    1. test valid topic publisher
    2. test invalid topic publisher
    3. test valid topic subscriber
    4. test invalid topic subscriber
    5. test valid queue sender
    6. test invalid queue sender
    7. test valid queue receiver
    8. test invalid queue receiver
    9. test valid queue browser.
    10. test invalid queue browser
    11. test preconf dur sub, to valid dest.
    12. test preconf dur sub, to invalid dest.
    13. test dyn dur sub, to valid dest.
    14. test  dyn dur sub, to valid dest.
    */
   public void runAuzValidTopicPublisher() throws Exception
   {
      TopicWorker pub1 = null;
      try
      {
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("USER_NR", 0);
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            c1,
            1);
         pub1.setUser("john", "needle");
         pub1.connect();
         pub1.publish();
      }catch(Exception ex)
      {
         Assert.fail("Could not publish to valid destination");
      }finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   public void runAuzInvalidTopicPublisher() throws Exception
   {
      TopicWorker pub1 = null;
      try
      {
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("USER_NR", 0);
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            c1,
            1);
         pub1.setUser("nobody", "nobody");
         pub1.connect();
         
         Exception e = null;
         try
         {
            pub1.publish();
         }catch(Exception ex)
         {
            e = ex;
         }
         log.debug(e);
         Assert.assertTrue("Unauz topic publishing throw wrong exception: "+e, e instanceof javax.jms.JMSSecurityException);
         
      }finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   public void runAuzValidTopicSubscriber() throws Exception
   {
      TopicWorker sub1 = null;
      try
      {
         drainTopic();
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
            "USER_NR",
            0,
            1);
         sub1 = new TopicWorker(SUBSCRIBER,
            TRANS_NONE,
            f1);
         
         sub1.setUser("john", "needle");
         Thread t1 = new Thread(sub1);
         t1.start();
         sleep(1000L);
         
         Exception ex = sub1.getException();
         
         t1.interrupt();
         Assert.assertTrue("Autz topic subscriber did not work", ex == null);
      }finally
      {
         try
         {
            sub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   public void runAuzInvalidTopicSubscriber() throws Exception
   {
      TopicWorker sub1 = null;
      try
      {
         drainTopic();
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
            "USER_NR",
            0,
            1);
         sub1 = new TopicWorker(SUBSCRIBER,
         TRANS_NONE,
         f1);
         
         sub1.setUser("nobody", "nobody");
         Thread t1 = new Thread(sub1);
         t1.start();
         sleep(1000L);
         
         Exception ex = sub1.getException();
         
         t1.interrupt();
         Assert.assertTrue("Unautz topic subscriber throw wrong exception: " +ex, ex instanceof javax.jms.JMSSecurityException);
      }finally
      {
         try
         {
            sub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   public void runAuzValidQueueSender() throws Exception
   {
      QueueWorker pub1 = null;
      try
      {
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("USER_NR", 0);
         pub1 = new QueueWorker(PUBLISHER,
            TRANS_NONE,
            c1,
            1);
         pub1.setUser("john", "needle");
         pub1.connect();
         pub1.publish();
      }catch(Exception ex)
      {
         Assert.fail("Could not publish to valid destination");
      }finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   public void runAuzInvalidQueueSender() throws Exception
   {
      QueueWorker pub1 = null;
      try
      {
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("USER_NR", 0);
         pub1 = new QueueWorker(PUBLISHER,
            TRANS_NONE,
            c1,
            1);
         pub1.setUser("nobody", "nobody");
         pub1.connect();
         
         Exception e = null;
         try
         {
            pub1.publish();
         }catch(Exception ex)
         {
            e = ex;
         }
         log.debug(e);
         Assert.assertTrue("Unauz queue publishing throw wrong exception: "+e, e instanceof javax.jms.JMSSecurityException);
         
      }finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   public void runAuzValidQueueReceiver() throws Exception
   {
      QueueWorker sub1 = null;
      try
      {         
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
            "USER_NR",
            0,
            1);
         sub1 = new QueueWorker(GETTER,
            TRANS_NONE,
            f1);
         
         sub1.setUser("john", "needle");
         sub1.connect();
         Exception ex = null;
         try
         {
            sub1.get();
         }catch(Exception e)
         {
            ex =e;
            log.error("ValidQueueReceiver got an exception: " + e,e);
         }
         Assert.assertTrue("Autz queue receiver did not work", ex == null);

      }finally
      {
         try
         {
            sub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   public void runAuzInvalidQueueReceiver() throws Exception
   {
      QueueWorker sub1 = null;
      try
      {         
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
            "USER_NR",
            0,
            1);
         sub1 = new QueueWorker(GETTER,
            TRANS_NONE,
            f1);
         
         sub1.setUser("nobody", "nobody");
         sub1.connect();
         Exception ex = null;
         try
         {
            sub1.get();
         }catch(Exception e)
         {
            ex =e;
         }
         Assert.assertTrue("Unautz queue receiver throw wrong exception: " +ex,
            ex instanceof javax.jms.JMSSecurityException);
      }finally
      {
         try
         {
            sub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   public void runAuzValidQueueBrowser() throws Exception
   {
      QueueWorker sub1 = null;
      try
      {
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
            "USER_NR",
            0,
            1);
         sub1 = new QueueWorker(GETTER,
            TRANS_NONE,
            f1);
         
         sub1.setUser("john", "needle");
         sub1.connect();
         Exception ex = null;
         try
         {
            sub1.browse();
         }catch(Exception e)
         {
            ex =e;
            log.error("ValidQueueBrowser throw exception: "+e,e);
         }
         Assert.assertTrue("Autz queue receiver did not work", ex == null);
      }finally
      {
         try
         {
            sub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   public void runAuzInvalidQueueBrowser() throws Exception
   {
      QueueWorker sub1 = null;
      try
      {         
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
            "USER_NR",
            0,
            1);
         sub1 = new QueueWorker(GETTER,
            TRANS_NONE,
            f1);
         
         sub1.setUser("nobody", "nobody");
         sub1.connect();
         Exception ex = null;
         try
         {
            sub1.browse();
         }catch(Exception e)
         {
            ex =e;
         }
         Assert.assertTrue("Unautz queue receiver throw wrong exception: " +ex, ex instanceof javax.jms.JMSSecurityException);
         
         
      }finally
      {
         try
         {
            sub1.close();
         }catch(Exception ex)
         {
            
         }
      }
   }
   
   public void runValidPreconfDurSub() throws Exception
   {
      TopicWorker sub1 = null;
      TopicWorker pub1 = null;
      try
      {
         // Clean testarea up
         drainTopic();
         
         int ic = 5;
         
         // Set up a durable subscriber
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
            "DURABLE_NR",
            0,
            ic);
         
         sub1 = new TopicWorker(SUBSCRIBER,
            TRANS_NONE,
            f1);
         sub1.setDurable("john", "needle", "sub2");
         Thread t1 = new Thread(sub1);
         t1.start();
         
         // Let is take some time to really set up the dur sub
         sleep(2000);
         // Publish
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("DURABLE_NR",0);
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            c1,
            ic);
         pub1.connect();
         pub1.publish();
         
         Assert.assertEquals("Publisher did not publish correct number of messages "+pub1.getMessageHandled(),
            ic,
            pub1.getMessageHandled());
         
         // let sub1 have some time to handle the messages.
         log.debug("Sleeping for " + ((ic*100)/60000) + " minutes");
         sleep(ic*100);
         
         Exception ex = sub1.getException();
         if (ex != null)
            log.error("ValidPreconfDurSub got an exception: " +ex,ex);
         Assert.assertTrue("ValidPreconfDurSub did not work", ex == null);
         
         Assert.assertEquals("Subscriber did not get correct number of messages "+sub1.getMessageHandled(),
            ic,
            sub1.getMessageHandled());
         
         t1.interrupt();
      }finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {}
         try
         {
            // if this stops working it might be that we have become spec
            // compliant an do not allow unsubscribe with an open consumer.
            sub1.unsubscribe();
            sub1.close();
         }catch(Exception ex)
         {}
      }
   }
   
   public void runInvalidPreconfDurSub() throws Exception
   {
      TopicWorker sub1 = null;
      try
      {
         // Clean testarea up
         TEST_TOPIC ="topic/securedTopic";
         //drainTopic();
         
         int ic = 5;
         
         // Set up a durable subscriber
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
            "DURABLE_NR",
            0,
            ic);
         
         sub1 = new TopicWorker(SUBSCRIBER,
            TRANS_NONE,
            f1);
         sub1.setDurable("john", "needle", "sub3");
         Thread t1 = new Thread(sub1);
         t1.start();
         
         // Let is take some time to really set up the dur sub
         sleep(2000);
         
         Exception ex = sub1.getException();
         Assert.assertTrue("InvalidPreconfDurSub did not get correct exception:" +ex, ex instanceof javax.jms.JMSSecurityException);
         
         t1.interrupt();
      }finally
      {
         try
         {
            
            sub1.close();
         }catch(Exception ex)
         {}
      }
   }
   public void runValidDynDurSub() throws Exception
   {
      TopicWorker sub1 = null;
      TopicWorker pub1 = null;
      try
      {
         // Clean testarea up
         drainTopic();
         
         int ic = 5;
         
         // Set up a durable subscriber
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
            "DURABLE_NR",
            0,
            ic);
         
         sub1 = new TopicWorker(SUBSCRIBER,
            TRANS_NONE,
            f1);
         sub1.setDurable("dynsub", "dynsub", "sub4");
         sub1.setClientID("myId");
         Thread t1 = new Thread(sub1);
         t1.start();
         
         // Let is take some time to really set up the dur sub
         sleep(2000);
         
         // Publish
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("DURABLE_NR",0);
         pub1 = new TopicWorker(PUBLISHER,
            TRANS_NONE,
            c1,
            ic);
         pub1.connect();
         pub1.publish();
         
         Assert.assertEquals("Publisher did not publish correct number of messages "+pub1.getMessageHandled(),
            ic,
            pub1.getMessageHandled());
         
         // let sub1 have some time to handle the messages.
         log.debug("Sleeping for " + ((ic*100)/60000) + " minutes");
         sleep(ic*100);
         
         Exception ex = sub1.getException();
         if (ex != null)
            log.error("ValidDynDurSub got an exception: " +ex,ex);
         Assert.assertTrue("ValidDynDurSub did not work", ex == null);
         
         Assert.assertEquals("Subscriber did not get correct number of messages "+sub1.getMessageHandled(),
            ic,
            sub1.getMessageHandled());

         t1.interrupt();
      }finally
      {
         try
         {
            pub1.close();
         }catch(Exception ex)
         {}
         try
         {
            // if this stops working it might be that we have become spec
            // compliant an do not allow unsubscribe with an open consumer.
            sub1.unsubscribe();
            sub1.close();
         }catch(Exception ex)
         {}
      }
   }
   
   public void runInvalidDynDurSub() throws Exception
   {
      TopicWorker sub1 = null;
      try
      {
         // Clean testarea up
         TEST_TOPIC ="topic/securedTopic";
         //drainTopic();
         
         int ic = 5;
         
         // Set up a durable subscriber
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
            "DURABLE_NR",
            0,
            ic);
         
         sub1 = new TopicWorker(SUBSCRIBER,
            TRANS_NONE,
            f1);
         sub1.setDurable("dynsub", "dynsub", "sub5");
         sub1.setClientID("myId2");
         Thread t1 = new Thread(sub1);
         t1.start();
         
         // Let is take some time to really set up the dur sub
         sleep(2000);
         
         Exception ex = sub1.getException();
         Assert.assertTrue("InvalidDynDurSub did not get correct exception:" +ex, ex instanceof javax.jms.JMSSecurityException);
         
         t1.interrupt();
      }finally
      {
         try
         {
            sub1.close();
         }catch(Exception ex)
         {}
      }
   }
   public static junit.framework.Test suite() throws Exception
   {
      
      TestSuite suite= new TestSuite();
      suite.addTest(new SecurityUnitTestCase("runLoginTest"));
      // Authentication tests
      suite.addTest(new SecurityUnitTestCase("runLoginNoCred"));
      suite.addTest(new SecurityUnitTestCase("runLoginValidCred"));
      suite.addTest(new SecurityUnitTestCase("runLoginInvalidPwd"));
      suite.addTest(new SecurityUnitTestCase("runLoginInvalidCred"));
      // ClientID tests
      suite.addTest(new SecurityUnitTestCase("runClientIDNormalTest"));
      suite.addTest(new SecurityUnitTestCase("runClientIDPreconfTest"));
      suite.addTest(new SecurityUnitTestCase("runClientIDSetTest"));
      suite.addTest(new SecurityUnitTestCase("runClientIDSetInternal"));
      suite.addTest(new SecurityUnitTestCase("runClientIDSetSteelPreconf"));
      suite.addTest(new SecurityUnitTestCase("runClientIDSetAfterInvoke"));
      // Autorization tests
      suite.addTest(new SecurityUnitTestCase("runAuzValidTopicPublisher"));
      suite.addTest(new SecurityUnitTestCase("runAuzInvalidTopicPublisher"));
      suite.addTest(new SecurityUnitTestCase("runAuzValidTopicSubscriber"));
      suite.addTest(new SecurityUnitTestCase("runAuzInvalidTopicSubscriber"));
      suite.addTest(new SecurityUnitTestCase("runAuzValidQueueSender"));
      suite.addTest(new SecurityUnitTestCase("runAuzInvalidQueueSender"));
      suite.addTest(new SecurityUnitTestCase("runAuzValidQueueReceiver"));
      suite.addTest(new SecurityUnitTestCase("runAuzInvalidQueueReceiver"));
      suite.addTest(new SecurityUnitTestCase("runAuzValidQueueBrowser"));
      suite.addTest(new SecurityUnitTestCase("runAuzInvalidQueueBrowser"));
      suite.addTest(new SecurityUnitTestCase("runValidPreconfDurSub"));
      suite.addTest(new SecurityUnitTestCase("runInvalidPreconfDurSub"));
      suite.addTest(new SecurityUnitTestCase("runValidDynDurSub"));
      suite.addTest(new SecurityUnitTestCase("runInvalidDynDurSub"));
      //suite.addTest(new DurableSubscriberTest("testBadClient"));

      // Create an initializer for the test suite
      Test wrapper = new JBossTestSetup(suite)
      {
         protected void tearDown() throws Exception
         {
            super.tearDown();

            // Remove all the messages created during this test
            getServer().invoke
            (
               new ObjectName("jboss.mq.destination:service=Queue,name=testQueue"),
               "removeAllMessages",
               new Object[0],
               new String[0]
            );
         }
      };
      return wrapper;
   }
   public static void main(String[] args)
   {
      
   }
   
} // SecurityTest
