/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq;

import java.util.Enumeration;

import javax.jms.Connection;
import javax.jms.DeliveryMode;
import javax.jms.Destination;
import javax.jms.ExceptionListener;
import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.MessageConsumer;
import javax.jms.MessageListener;
import javax.jms.MessageProducer;
import javax.jms.Queue;
import javax.jms.QueueBrowser;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueSender;
import javax.jms.QueueSession;
import javax.jms.Session;
import javax.jms.Topic;
import javax.jms.TopicConnection;
import javax.jms.TopicConnectionFactory;
import javax.jms.TopicPublisher;
import javax.jms.TopicSession;
import javax.naming.Context;
import javax.naming.NamingException;

import org.jboss.test.JBossTestCase;
import org.apache.log4j.Category;

/**
 * JMS tests base class.
 *
 * Your test extends this class, and can then use common methods. To do
 * the tests you use TopicWorker or QueueWorker and the MessageCreator,
 * MessageFilter and perhaps MessageQos classes, directly or by extending
 * them.
 *
 * You can change the connection factories and destinations used by the
 * properties:   jbosstest.queuefactory, jbosstest.topicfactory, 
 * jbosstest.queue or jbosstest.topic.
 *
 *
 * @author    <a href="pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.1.4.1 $
 */
public class MQBase extends JBossTestCase
{
   public static final int PUBLISHER = 0;
   public static final int SUBSCRIBER = 1;
   public static final int GETTER = 2;
   public static final int CONNECTOR = 3;
   public static final int FAILSAFE_SUBSCRIBER = 4;
   public static final int TRANS_NONE = 0;
   public static final int TRANS_INDIVIDUAL = 1;
   public static final int TRANS_TOTAL = 2;
   public static final String[] TRANS_DESC = {"NOT", "individually", "totally"};
   public static final int DEFAULT_RUNSLEEP = 50;
   public final Category log = getLog();

   // Provider specific
   public String TOPIC_FACTORY = "ConnectionFactory";
   public String QUEUE_FACTORY = "ConnectionFactory";

   public String TEST_QUEUE = "queue/testQueue";
   public String TEST_TOPIC = "topic/testTopic";

   public Context context;
   public QueueConnectionFactory queueFactory;
   public TopicConnectionFactory topicFactory;

   public MQBase(String name)
   {
      super(name);
   }

   public long getRunSleep()
   {
      log.info("run.sleep: " + System.getProperty("run.sleep"));
      return 1000L * Integer.getInteger("run.sleep", DEFAULT_RUNSLEEP).intValue();
   }

   public void sleep(long sleep)
   {
      try
      {
         Thread.sleep(sleep);
      }
      catch (InterruptedException e)
      {
      }
   }

   public void drainTopic() throws JMSException
   {
      TopicWorker sub1 = new TopicWorker(GETTER,
         TRANS_NONE,
         null
      );
      sub1.connect();
      sub1.get();
      sub1.close();
   }

   public void drainQueue() throws JMSException
   {
      QueueWorker sub1 = new QueueWorker(GETTER,
         TRANS_NONE,
         null
      );
      sub1.connect();
      sub1.get();
      sub1.close();
   }

   /**
    * The JUnit setup method
    *
    * @exception Exception  Description of Exception
    */
   protected void setUp() throws Exception
   {
      // Reconfigure acording to props
      QUEUE_FACTORY = System.getProperty("jbosstest.queuefactory", QUEUE_FACTORY);
      TOPIC_FACTORY = System.getProperty("jbosstest.topicfactory", TOPIC_FACTORY);
      TEST_QUEUE = System.getProperty("jbosstest.queue", TEST_QUEUE);
      TEST_TOPIC = System.getProperty("jbosstest.topic", TEST_TOPIC);

      if (context == null)
      {

         context = getInitialContext();

         queueFactory = (QueueConnectionFactory) context.lookup(QUEUE_FACTORY);
         topicFactory = (TopicConnectionFactory) context.lookup(TOPIC_FACTORY);

         getLog().debug("Connection to JBossMQ established.");
      }

   }


   public static void main(String[] args)
   {

   }

   public abstract class JMSWorker implements Runnable, MessageListener, ExceptionListener
   {

      protected boolean stopRequested = false;
      protected int messageHandled = 0;
      protected Exception runEx = null;
      protected MessageFilter filter;
      protected MessageCreator creator;
      protected int number = 1;
      protected int type = -1;
      protected int transacted;
      protected QosConfig qosConfig = new QosConfig();
      protected String userName;
      protected String password;
      protected String clientID;

      // Generic ones, should be set by sublcasses
      public Connection connection;
      public Destination destination;
      public Session session;
      public MessageProducer producer;
      public MessageConsumer consumer;

      /**
       * Create one without any settings, use mutators instead. Makes it easier to owerride.
       */
      public JMSWorker()
      {
      }

      public JMSWorker(int type, int transacted, MessageFilter filter)
      {
         this.type = type;
         this.transacted = transacted;
         this.filter = filter;
      }

      public JMSWorker(int type,
         int transacted,
         MessageCreator creator,
         int number
         )
      {
         this.type = type;
         this.transacted = transacted;
         this.creator = creator;
         this.number = number;
      }

      public void setSubscriberAttrs(int type, int transacted, MessageFilter filter)
      {
         this.type = type;
         this.transacted = transacted;
         this.filter = filter;
      }

      public void setPublisherAttrs(int type,
         int transacted,
         MessageCreator creator,
         int number)
      {
         this.type = type;
         this.transacted = transacted;
         this.creator = creator;
         this.number = number;
      }

      public void setUser(String userName, String password)
      {
         this.userName = userName;
         this.password = password;
      }

      public void setClientID(String ID)
      {
         this.clientID = ID;
      }

      abstract public void publish() throws JMSException;

      abstract public void publish(int nr) throws JMSException;

      /**
       * Subsribes, collects, checking any set filters. A messageComsumer must be created before calling this.
       */
      public void subscribe() throws JMSException
      {
         subscribe(false);
      }

      /**
       * Subsribes, collects, checking any set filters. A messageComsumer must be created before calling this. If arg set to true, do a failsafe sub
       */
      public void subscribe(boolean failsafe) throws JMSException
      {
         if (consumer == null)
            throw new JMSException("No messageConsumer created");

         if (failsafe)
            connection.setExceptionListener(this);

         consumer.setMessageListener(this);

      }

      public void get() throws JMSException
      {
         Message msg = consumer.receive(2000);
         while (msg != null)
         {
            if (filter != null)
            {
               if (filter.ok(msg))
                  messageHandled++;
            }
            else
            {
               messageHandled++;
            }
            msg = consumer.receive(2000);
         }
      }

      abstract public void connect() throws JMSException;

      public void setQosConfig(QosConfig qosConfig)
      {
         this.qosConfig = qosConfig;
      }

      public void setStoped() throws JMSException
      {
         stopRequested = true;
      }

      public int getMessageHandled()
      {
         return messageHandled;
      }

      public Exception getException()
      {
         return runEx;
      }

      public void reset()
      {
         messageHandled = 0;
         stopRequested = false;
         runEx = null;
      }

      public void close()
      {
         try
         {
            if (consumer != null)
               consumer.close();
            if (producer != null)
               producer.close();
            if (session != null)
               session.close();
         }
         catch (JMSException ex)
         {
         }
         finally
         {
            if (connection != null)
            {
               try
               {
                  connection.close();
               }
               catch (JMSException ex)
               {
               }
            }
         }
      }

      public void onMessage(Message msg)
      {
         try
         {
            if (filter != null)
            {
               if (filter.ok(msg))
                  messageHandled++;
            }
            else
            {
               messageHandled++;
            }
         }
         catch (Exception ex)
         {
            log.warn("Exception in on message: " + ex, ex);
            runEx = ex;
         }
      }

      /**
       * onException handling is only for subscriber. Will try to to
       * a connect followed by a subscribe
       */
      public void onException(JMSException ex)
      {
         log.error("Ex in connection: " + ex);

         try
         {
            connection.setExceptionListener(null);
            close();
         }
         catch (JMSException c)
         {
         }
         
         // Try reconnect, loops until success or shut down
         try
         {
            boolean tryIt = true;
            while (tryIt && !stopRequested)
            {
               log.info("Trying reconnect...");
               try
               {
                  Thread.sleep(10000);
               }
               catch (InterruptedException ie)
               {
               }
               try
               {
                  connect();
                  subscribe(true);
                  tryIt = false;
                  log.info("Reconnect OK");
                  //return;
               }
               catch (JMSException e)
               {
                  log.error("Error in reconnect: " + e);
               }
            }

         }
         catch (Exception je)
         {
            log.error("Strange error in failsafe handling" + je, je);
         }
      }

      public void run()
      {
         try
         {
            switch (type)
            {
               case -1:
                  log.info("Nothing to do for type " + type);
                  break;
               case PUBLISHER:
                  connect();
                  publish();
                  break;
               case SUBSCRIBER:
                  connect();
                  subscribe();
                  break;
               case GETTER:
                  connect();
                  get();
                  break;
               case CONNECTOR:
                  connect();
                  break;
               case FAILSAFE_SUBSCRIBER:
                  connect();
                  subscribe(true);
                  break;
            }

            //if the method does not hold an own thread, we do it here
            while (!stopRequested)
            {
               try
               {
                  Thread.sleep(1000);
               }
               catch (InterruptedException ex)
               {

               }
            }
         }
         catch (JMSException ex)
         {
            runEx = ex;
            log.error("Could not run: " + ex, ex);
         }
      }
   }

   public interface MessageCreator
   {
      public void setSession(Session session);

      public Message createMessage(int nr) throws JMSException;
   }

   public abstract class BaseMessageCreator implements MessageCreator
   {
      protected Session session;
      protected String property;

      public BaseMessageCreator(String property)
      {
         this.property = property;
      }

      public void setSession(Session session)
      {
         this.session = session;
      }

      abstract public Message createMessage(int nr) throws JMSException;
   }


   public class IntRangeMessageCreator extends BaseMessageCreator
   {
      int start = 0;

      public IntRangeMessageCreator(String property)
      {
         super(property);
      }

      public IntRangeMessageCreator(String property, int start)
      {
         super(property);
         this.start = start;
      }

      public Message createMessage(int nr) throws JMSException
      {
         if (session == null)
            throw new JMSException("Session not allowed to be null");

         Message msg = session.createMessage();
         msg.setStringProperty(property, String.valueOf(start + nr));
         return msg;
      }
   }

   public interface MessageFilter
   {
      public boolean ok(Message msg) throws JMSException;
   }

   public class IntRangeMessageFilter implements MessageFilter
   {
      Class messageClass;
      String className;
      String property;
      int low;
      int max;
      int counter = 0;
      int report = 1000;

      public IntRangeMessageFilter(Class messageClass, String property, int low, int max)
      {
         this.messageClass = messageClass;
         this.property = property;
         className = messageClass.getName();
         this.low = low;
         this.max = max;
      }

      private boolean validateClass(Message msg)
      {
         Class clazz = null;
         if (msg instanceof javax.jms.TextMessage)
            clazz = javax.jms.TextMessage.class;
         else if (msg instanceof javax.jms.BytesMessage)
            clazz = javax.jms.BytesMessage.class;
         else if (msg instanceof javax.jms.MapMessage)
            clazz = javax.jms.MapMessage.class;
         else if (msg instanceof javax.jms.ObjectMessage)
            clazz = javax.jms.ObjectMessage.class;
         else if (msg instanceof javax.jms.StreamMessage)
            clazz = javax.jms.StreamMessage.class;
         else if (msg instanceof javax.jms.Message)
            clazz = javax.jms.Message.class;
         else
            return false;//Not a recognized message typ

         return clazz.equals(messageClass);
      }

      public boolean ok(Message msg) throws JMSException
      {
         boolean res = false;
         if (validateClass(msg))
         {
            if (msg.propertyExists(property))
            {
               String p = msg.getStringProperty(property);
               try
               {
                  int i = Integer.parseInt(p);
                  //log.debug("Received message " + property +"=" +i);
                  if (i >= low && i < max)
                     res = true;
               }
               catch (NumberFormatException ex)
               {
                  throw new JMSException("Property " + property + " was not int: " + p);
               }
            }
         }
         counter++;
         int mod = counter % report;
         if (mod == 0)
            log.debug("Have received " + counter + " messages");
         return res;
      }

   }

   /*  
   public class REMessageFilter implements MessageFilter {
      Class messageClass;
      String className;
      String property;
      RE re = null;
      public REMessageFilter(Class messageClass, String property, String regexp) throws REException{
         this.messageClass = messageClass;
         this.property = property;
         re = new RE(regexp);
         className = messageClass.getName();
      }
      
      public boolean ok(Message msg) throws JMSException{
         boolean res = false;
         if (className.equals(msg.getClass().getName())) {
            if (msg.propertyExists(property)) {
               String p = msg.getStringProperty(property);
               if (re.getMatch(p)!=null)
                  res = true;
            } 
         }
         return true;
      }
   }
   */
   /**
    * Defines quality of service for message publishing. Defaults are the same
    * ase defined in SpyMessage.
    */
   public class QosConfig
   {
      int deliveryMode = DeliveryMode.PERSISTENT;
      int priority = 4;
      long ttl = 0;
   }

   public class TopicWorker extends JMSWorker
   {
      String durableHandle;

      /**
       * If using this, use mutators to add attrs.
       */
      public TopicWorker()
      {
         super();
      }

      public TopicWorker(int type, int transacted, MessageFilter filter)
      {
         super(type, transacted, filter);
      }

      public TopicWorker(int type,
         int transacted,
         MessageCreator creator,
         int number
         )
      {
         super(type, transacted, creator, number);
      }


      public void publish() throws JMSException
      {
         publish(number);
      }

      public void publish(int nr) throws JMSException
      {
         if (producer == null)
            producer = ((TopicSession) session).createPublisher((Topic) destination);
         if (creator == null)
            throw new JMSException("Publish must have a MessageCreator set");

         creator.setSession(session);
         log.debug("Publishing " + nr + " messages");
         for (int i = 0; i < nr; i++)
         {
            if (qosConfig != null)
            {
               ((TopicPublisher) producer).publish(creator.createMessage(i),
                  qosConfig.deliveryMode,
                  qosConfig.priority,
                  qosConfig.ttl);
            }
            else
            {
               ((TopicPublisher) producer).publish(creator.createMessage(i));
            }

            messageHandled++;
         }
         log.debug("Finished publishing");
      }

      public void subscribe() throws JMSException
      {
         subscribe(false);
      }

      public void subscribe(boolean failsafe) throws JMSException
      {
         if (durableHandle != null)
            consumer = ((TopicSession) session).createDurableSubscriber((Topic) destination, durableHandle);
         else
            consumer = ((TopicSession) session).createSubscriber((Topic) destination);
         super.subscribe(failsafe);
         connection.start();
      }

      public void get() throws JMSException
      {
         consumer = ((TopicSession) session).createSubscriber((Topic) destination);
         super.subscribe();
         connection.start();
      }

      public void connect() throws JMSException
      {
         log.debug("Connecting: " + this.toString());
         if (userName != null)
            connection = topicFactory.createTopicConnection(userName, password);
         else
            connection = topicFactory.createTopicConnection();

         if (clientID != null)
         {
            log.debug("Setting clientID" + clientID);
            connection.setClientID(clientID);
         }

         session = ((TopicConnection) connection).createTopicSession(transacted != TRANS_NONE, Session.AUTO_ACKNOWLEDGE);
         try
         {
            destination = (Destination) context.lookup(TEST_TOPIC);
         }
         catch (NamingException ex)
         {
            throw new JMSException("Could not lookup topic " + ex);
         }
      }

      // Topic specific stuff
      public void setDurable(String userId, String pwd, String handle)
      {
         this.userName = userId;
         this.password = pwd;
         this.durableHandle = handle;
      }

      public void setDurable(String handle)
      {
         this.durableHandle = handle;
      }

      public void unsubscribe() throws JMSException
      {
         if (durableHandle != null)
            ((TopicSession) session).unsubscribe(durableHandle);
      }

      public String toString()
      {
         return "(userId=" + userName + " pwd=" + password + " handle=" + durableHandle + ")";
      }

   }

   public class QueueWorker extends JMSWorker
   {
      String userId;
      String pwd;
      String handle;

      /**
       * If using this, use mutators to add attrs.
       */
      public QueueWorker()
      {
         super();
      }

      public QueueWorker(int type, int transacted, MessageFilter filter)
      {
         super(type, transacted, filter);
      }

      public QueueWorker(int type,
         int transacted,
         MessageCreator creator,
         int number
         )
      {
         super(type, transacted, creator, number);
      }


      public void publish() throws JMSException
      {
         publish(number);
      }

      public void publish(int nr) throws JMSException
      {
         if (producer == null)
            producer = ((QueueSession) session).createSender((Queue) destination);
         if (creator == null)
            throw new JMSException("Publish must have a MessageCreator set");

         creator.setSession(session);
         log.debug("Publishing " + nr + " messages");
         for (int i = 0; i < nr; i++)
         {
            if (qosConfig != null)
            {
               ((QueueSender) producer).send(creator.createMessage(i),
                  qosConfig.deliveryMode,
                  qosConfig.priority,
                  qosConfig.ttl);
            }
            else
            {
               ((QueueSender) producer).send(creator.createMessage(i));
            }

            messageHandled++;
         }
         log.debug("Finished publishing");
      }

      public void subscribe() throws JMSException
      {
         subscribe(false);
      }

      public void subscribe(boolean failsafe) throws JMSException
      {

         consumer = ((QueueSession) session).createReceiver((Queue) destination);
         super.subscribe(failsafe);
         connection.start();
      }

      public void get() throws JMSException
      {
         consumer = ((QueueSession) session).createReceiver((Queue) destination);
         super.subscribe();
         connection.start();
      }

      public void connect() throws JMSException
      {
         log.debug("Connecting: " + this.toString());
         if (userName != null)
            connection = queueFactory.createQueueConnection(userName, password);
         else
            connection = queueFactory.createQueueConnection();

         if (clientID != null)
            connection.setClientID(clientID);

         session = ((QueueConnection) connection).createQueueSession(transacted != TRANS_NONE, Session.AUTO_ACKNOWLEDGE);
         try
         {
            destination = (Destination) context.lookup(TEST_QUEUE);
         }
         catch (NamingException ex)
         {
            throw new JMSException("Could not lookup topic " + ex);
         }
      }


      // Queue specific
      public Enumeration browse() throws JMSException
      {
         QueueBrowser b = ((QueueSession) session).createBrowser((Queue) destination);
         return b.getEnumeration();
      }
   }
} // MQBase
