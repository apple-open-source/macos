package org.jboss.test.cts.jms;

import java.io.*;
import java.util.*;
import java.util.Date;
import javax.naming.*;
import javax.jms.*;

public class MsgSender
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
  public final static String JMS_FACTORY="ConnectionFactory";
  public final static String QUEUE="queue/testQueue";

  private QueueConnectionFactory qconFactory;
  private QueueConnection qcon;
  private QueueSession qsession;
  private QueueSender qsender;
  private TextMessage msg;
  private Queue queue;

  public MsgSender ( )
  {
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
    qcon.start();
  }

  /**
   * Close JMS objects.
   */
  public void close()
       throws JMSException
  {
  	if( qcon != null ) {
    	qsender.close();
    	qsession.close();
    	qcon.close();
		qcon = null;
	}
  }

  public void sendMsg( String message )
  {
    try
    {
	  init( new InitialContext(), QUEUE);
      log.debug("Sending a message.." );
      qsender = qsession.createSender(queue);
      msg = qsession.createTextMessage();
      msg.setText(message);
      qsender.send(msg);
	  close();
    }
    catch(Exception ex)
    {
	ex.printStackTrace( );
    }
  }

  private static InitialContext getInitialContext(String url)
       throws NamingException
  {
      //Hashtable env = new Hashtable();
      //env.put(Context.INITIAL_CONTEXT_FACTORY, JNDI_FACTORY);
      //env.put(Context.PROVIDER_URL, url);
      //return new InitialContext(env);
      return new InitialContext();
  }

}






