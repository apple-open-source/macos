/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.blocks.ejb;

import javax.ejb.MessageDrivenBean;
import javax.ejb.MessageDrivenContext;
import javax.ejb.EJBContext;
import javax.ejb.EJBException;

import javax.jms.Message;
import javax.jms.MessageListener;

import org.jboss.logging.Logger;

import org.jboss.util.NotImplementedException;

/**
 * A base support class for an <em>EJB message driven bean</em>.
 *
 * <p>Fulfills all methods required for the {@link MessageDrivenBean} interface
 *    including basic {@link MessageDrivenContext} handling.
 *
 * <p>Provides a convience indirection on top of {@link #onMessage}, which 
 *    will log any exception thrown.  To make use of this simply override
 *    {@link #process} to process the message.
 *
 * @see javax.ejb.MessageDrivenBean
 *
 * @version <tt>$Revision: 1.1.1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class MessageDrivenBeanSupport
   implements MessageDrivenBean, MessageListener
{
   /** Instance logger. */
   protected Logger log = Logger.getLogger(getClass());
   
   /** Message driven context. */
   private MessageDrivenContext messageDrivenContext;

   /**
    * Set the message driven context.
    *
    * @param context    Message driven context.
    */
   public void setMessageDrivenContext(final MessageDrivenContext context)
      throws EJBException
   {
      messageDrivenContext = context;
   }

   /**
    * Get the message driven context.
    *
    * @return  Message driven context.
    *
    * @throws IllegalStateException    Message driven context is invalid.
    */
   public MessageDrivenContext getMessageDrivenContext()
      throws EJBException
   {
      if (messageDrivenContext == null)
         throw new IllegalStateException("message driven context is invalid");

      return messageDrivenContext;
   }

   /**
    * Get the EJB context.
    *
    * @return  EJB context.
    */
   protected EJBContext getEJBContext()
      throws EJBException
   {
      return getMessageDrivenContext();
   }

   /**
    * Non-operation.
    */
   public void ejbRemove() throws EJBException {}

   /**
    * Override with custom message processing.
    *
    * @throws NotImplementedException
    */
   protected void process(final Message msg) throws Exception {
      throw new NotImplementedException();
   }

   /**
    * Invokes {@link #process}, logs all exceptions thrown.
    *
    * @param msg  The message to process.
    */
   public void onMessage(final Message msg) {
      try {
         process(msg);
      }
      catch (Exception e) {
         log.error("Failed to process message", e);
      }
   }
}
