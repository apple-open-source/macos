/*
 * Copyright (c) 2000 Peter Antman DN <peter.antman@dn.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
package org.jboss.test.jmsra.bean;

import javax.ejb.MessageDrivenBean;
import javax.ejb.MessageDrivenContext;
import javax.ejb.EJBException;

import javax.naming.*;
import javax.jms.*;

import org.apache.log4j.Category;

/**
 * Listen on a topic, send to queue
 *
 *
 * Created: Sat Nov 25 18:07:50 2000
 *
 * @author 
 * @version
 */

public class TopicAdapter implements MessageDrivenBean, MessageListener{

    private static final Category log = Category.getInstance(TopicAdapter.class);
   
    private static final String CONNECTION_JNDI = "java:comp/env/jms/MyQueueConnection";
    private static final String QUEUE_JNDI = "java:comp/env/jms/QueueName";
    private MessageDrivenContext ctx = null;
    private Queue queue = null;
    private QueueConnection queueConnection = null;

    public TopicAdapter() {
	
    }
    public void setMessageDrivenContext(MessageDrivenContext ctx)
    {
	this.ctx = ctx;
    }
    
    public void ejbCreate() {
	try {
            Context context = new InitialContext();
            queue = (Queue)context.lookup(QUEUE_JNDI);

	    QueueConnectionFactory factory = (QueueConnectionFactory)context.lookup(CONNECTION_JNDI);
	    queueConnection = factory.createQueueConnection();
	    
        } catch (Exception ex) {
            // JMSException or NamingException could be thrown
            log.debug("failed", ex);
	    throw new EJBException(ex.toString());
        }
    }

    public void ejbRemove() {
	if(queueConnection != null) {
            try {
                queueConnection.close();
            } catch (Exception e) {
                log.debug("failed", e);
            }
        }
	ctx=null;
    }

    public void onMessage(Message message) {
	log.debug("TopicBean got message" + message.toString() );
	QueueSession   queueSession = null;
	try {
	    QueueSender queueSender = null;
	    
            queueSession = 
                queueConnection.createQueueSession(true, Session.AUTO_ACKNOWLEDGE);
            queueSender = queueSession.createSender(queue);
	    queueSender.send(message);
	    
	    
        } catch (JMSException ex) {
	    
            log.debug("failed", ex);
            ctx.setRollbackOnly();
	    throw new EJBException(ex.toString());
        } finally {
            if (queueSession != null) {
                try {
                    queueSession.close();
                } catch (Exception e) {
                    log.debug("failed", e);
                }
            }
        }
    }
} // MessageBeanImpl
