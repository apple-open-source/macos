/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cts.jms;

import java.io.*;
import java.util.*;
import java.util.Date;
import javax.naming.*;
import javax.jms.*;

import org.apache.log4j.Category;

public class ContainerMBox
  implements MessageListener
{
  public final static String JMS_FACTORY="ConnectionFactory";
  public final static String QUEUE="queue/testQueue";

  private QueueConnectionFactory qconFactory;
  private QueueConnection qcon;
  private QueueSession qsession;
  private QueueReceiver qreceiver;
  private TextMessage msg;
  private Queue queue;

  private Category log;

  public static final String EJB_CREATE_MSG = "EJB_CREATE_MSG";
  public static final String EJB_POST_CREATE_MSG = "EJB_POST_CREATE_MSG";
  public static final String EJB_ACTIVATE_MSG = "EJB_ACTIVATE_MSG";
  public static final String EJB_PASSIVATE_MSG = "EJB_PASSIVATE_MSG";
  public static final String EJB_REMOVE_MSG = "EJB_REMOVE_MSG";
  public static final String EJB_LOAD_MSG = "EJB_LOAD_MSG";
  public static final String EJB_STORE_MSG = "EJB_STORE_MSG";
  public static final String SET_ENTITY_CONTEXT_MSG = "SET_ENTITY_CONTEXT_MSG";
  public static final String UNSET_ENTITY_CONTEXT_MSG = "UNSET_ENTITY_CONTEXT_MSG";

  private HashMap messageList = new HashMap( );

  public ContainerMBox ( )
  {
    log = Category.getInstance(getClass().getName());
    try
    {
       init( new InitialContext(), QUEUE );
    }
    catch(Exception ex)
    {
       log.error("MBox could not get initial context", ex);
    }
  }

  // MessageListener interface
  public void onMessage(Message msg)
  {
    try 
    {
      String msgText;
      if (msg instanceof TextMessage) 
      {
        msgText = ((TextMessage)msg).getText();
      } 
      else 
      {
        msgText = msg.toString();
      }

      log.debug("[BEAN MESSAGE]: "+ msgText );
      messageList.put(msgText, "msg" );
    } 
    catch (JMSException jmse) 
    {
      log.error("problem receiving MBox message", jmse);
    }
  }

  /**
   * Create all the necessary objects for receiving
   * messages from a JMS queue.
   */
  public void init(Context ctx, String queueName)
       throws NamingException, JMSException
  {
    qconFactory = (QueueConnectionFactory) ctx.lookup(JMS_FACTORY);
    qcon = qconFactory.createQueueConnection();
    qsession = qcon.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
    try 
    {
      queue = (Queue) ctx.lookup(queueName);
    } 
    catch (NamingException ne) 
    {
      queue = qsession.createQueue(queueName);
      ctx.bind(queueName, queue);
    }
    qreceiver = qsession.createReceiver(queue);
    qreceiver.setMessageListener(this);
    qcon.start();
  }

  /**
   * Close JMS objects.
   */
  public void close()
       throws JMSException
  {
    qreceiver.close();
    qsession.close();
    qcon.close();
  }

  public boolean messageReceived( String message )
  {
      return messageList.containsKey(message);
  }

  public void clearMessages( )
  {
      messageList = null;
      messageList = new HashMap();
  }
  
  private static InitialContext getInitialContext(String url)
       throws NamingException
  {
      return new InitialContext();
  }

}






