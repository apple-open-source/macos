/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.il.ha.examples;

import javax.jms.ExceptionListener;
import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.MessageListener;
import javax.jms.Queue;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueReceiver;
import javax.jms.QueueSender;
import javax.jms.QueueSession;
import javax.jms.TextMessage;
import javax.jms.Topic;
import javax.jms.TopicConnection;
import javax.jms.TopicConnectionFactory;
import javax.jms.TopicPublisher;
import javax.jms.TopicSession;
import javax.jms.TopicSubscriber;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.jboss.logging.Logger;
import org.jboss.system.ServiceMBeanSupport;

/**
 * 
 * Helps to manually test the HAIL 
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public class HAJMSClient extends ServiceMBeanSupport
  implements MessageListener, ExceptionListener, HAJMSClientMBean
{

  /**
   * create connection, sessions and subscribe for topic and queue 
   */
  protected void startService() throws Exception 
  {
    connect();
  }
   
  /**
   * unsubscribe from topic, queue,
   *  stop sessions and connection  
   */
  protected void stopService() throws Exception 
  {
    disconnect();
  }

  /**
   * Acknowledges connenction exception.
   * Should be invoked every time the HAIL singleton moves.
   */
  public void onException(JMSException connEx)
  {
    log.info("Notification received by ExceptionListener. Singleton Probably Moved.");
    try
    {
      reconnect();
    }
    catch (Exception e)
    {
      e.printStackTrace();
    }
    finally
    {
      connectionException_ = connEx;
    }
  }

  protected void reconnect() throws NamingException, JMSException 
  {
    log.info("Reconnecting");
    try
    {
      disconnect();
    }
    finally
    {
      connect();
    }
  }

  public void connect() throws NamingException, JMSException
  {
    log.info("Connecting");

    InitialContext iniCtx = new InitialContext();
    Object tmp = iniCtx.lookup("HAILXAConnectionFactory");
    
    TopicConnectionFactory tcf = (TopicConnectionFactory)tmp;
    topicConn_ = tcf.createTopicConnection(); 
    topic_ = (Topic)iniCtx.lookup("topic/testTopic");
    topicSession_ = topicConn_.createTopicSession(false, TopicSession.AUTO_ACKNOWLEDGE);
    topicConn_.setExceptionListener(this);
    topicSub_ = topicSession_.createSubscriber(topic_);
    topicSub_.setMessageListener( this );
    topicPub_ = topicSession_.createPublisher(topic_);
    topicConn_.start();

    QueueConnectionFactory qcf = (QueueConnectionFactory)tmp;
    qConn_ = qcf.createQueueConnection(); 
    q_ = (Queue)iniCtx.lookup("queue/testQueue");
    qSession_ = qConn_.createQueueSession(false, QueueSession.AUTO_ACKNOWLEDGE);
    qRecv_ = qSession_.createReceiver(q_);
    qRecv_.setMessageListener( this );
    qSend_ = qSession_.createSender(q_);
    qConn_.start();

    log.info("Connected");
  }

  public void disconnect() throws JMSException
  {
    if (topicConn_ == null) return;

    log.info("Disconnecting");

    connectionException_ = null;
    
    try
    {
    topicConn_.setExceptionListener(null);

    topicSub_.close();    
    topicPub_.close();    
    topicConn_.stop();
    topicSession_.close();
    
    qRecv_.close();
    qSend_.close();
    qConn_.stop();
    qSession_.close();
    }
    finally
    {
      try
      {
        topicConn_.close();
      }
      finally
      {
        topicConn_ = null;
        try
        {
          qConn_.close();
        }
        finally
        {
          qConn_ = null;
        }
      }
    
    }
    log.info("Disconnected");
  }
  
  /**
   * Handle JMS message
   * 
   * @see javax.jms.MessageListener#onMessage(javax.jms.Message)
   */
  public void onMessage(Message msg)
  {
    lastMessage_ = (TextMessage)msg;
    log.info("Message received: " + msg);
  }
  
  public String getLastMessage() throws JMSException
  {
    if (lastMessage_ == null) return null;
    return lastMessage_.getText();
  }
  
  public String getConnectionException()
  {
    if (connectionException_ == null) return null;
    return connectionException_.toString();
  }
  
  public void publishMessageToTopic(String text) throws JMSException
  {
    TextMessage msg = topicSession_.createTextMessage(text);
    topicPub_.publish(msg);
    log.info("HA JMS message published to topic: " + text);
  }
  
  public void sendMessageToQueue(String text) throws JMSException
  {
    TextMessage msg = qSession_.createTextMessage(text);
    qSend_.send(msg);
    log.info("HA JMS message sent to queue: " + text);
  }

  private Topic topic_;  
  private TopicSession topicSession_;
  private TopicConnection topicConn_;
  private JMSException connectionException_;
  private TopicSubscriber topicSub_;
  private TopicPublisher topicPub_;
  private TextMessage lastMessage_;

  private Queue q_;
  private QueueConnection qConn_;
  private QueueSession qSession_;
  private QueueReceiver qRecv_;
  private QueueSender qSend_;

  private static Logger log = Logger.getLogger(HAJMSClient.class);
  
}
