/*
 * jBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.security.ejb;

import javax.ejb.MessageDrivenBean;
import javax.ejb.MessageDrivenContext;
import javax.ejb.EJBException;
import javax.jms.MessageListener;
import javax.jms.Message;
import javax.jms.Queue;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueReceiver;
import javax.jms.QueueSender;
import javax.jms.QueueSession;
import javax.jms.Session;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.jboss.test.security.interfaces.Entity;
import org.jboss.test.security.interfaces.EntityHome;

/** An MDB that takes the string from the msg passed to onMessage
 and invokes the echo(String) method on an internal Entity using
 the InternalRole assigned in the MDB descriptor run-as element.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.7 $
 */
public class RunAsMDB implements MessageDrivenBean, MessageListener
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
   private MessageDrivenContext ctx = null;
   private InitialContext iniCtx;
   
   public RunAsMDB()
   {
   }

   public void setMessageDrivenContext(MessageDrivenContext ctx)
      throws EJBException
   {
      this.ctx = ctx;
      try
      {
         iniCtx = new InitialContext();
      }
      catch(NamingException e)
      {
         throw new EJBException(e);
      }
   }
   
   public void ejbCreate()
   {
   }
   
   public void ejbRemove()
   {
      ctx = null;
   }

   public void onMessage(Message message)
   {
      Queue replyTo = null;
      try
      {
         replyTo = (Queue) message.getJMSReplyTo();
         String arg = message.getStringProperty("arg");
         EntityHome home = (EntityHome) iniCtx.lookup("java:comp/env/ejb/Entity");
         Entity bean = home.findByPrimaryKey(arg);
         String echo = bean.echo(arg);
         log.debug("RunAsMDB echo("+arg+") -> "+echo);
         sendReply(replyTo, arg);
      }
      catch(Throwable e)
      {
         log.debug("failed", e);
         if( replyTo != null )
            sendReply(replyTo, "Failed, ex="+e.getMessage());
      }
   }
   private void sendReply(Queue replyTo, String info)
   {
      try
      {
         InitialContext ctx = new InitialContext();
         QueueConnectionFactory queueFactory = (QueueConnectionFactory) ctx.lookup("java:comp/env/jms/QueFactory");
         QueueConnection queueConn = queueFactory.createQueueConnection();
         QueueSession session = queueConn.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
         Message msg = session.createMessage();
         msg.setStringProperty("reply", info);
         QueueSender sender = session.createSender(replyTo);
         sender.send(msg);
         sender.close();
         session.close();
         queueConn.close();
      }
      catch(Exception e)
      {
         log.error("Failed to send reply", e);
      }
   }
}
