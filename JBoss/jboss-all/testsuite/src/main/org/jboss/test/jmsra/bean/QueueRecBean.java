/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package  org.jboss.test.jmsra.bean;

import java.rmi.RemoteException;

import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.ejb.EJBException;

import javax.naming.InitialContext;
import javax.naming.Context;

import javax.jms.QueueConnectionFactory;
import javax.jms.QueueConnection;
import javax.jms.QueueSession;
import javax.jms.QueueReceiver;
import javax.jms.Queue;
import javax.jms.Session;
import javax.jms.Message;
import javax.jms.JMSException;

import org.apache.log4j.Category;


/**
 * <p>QueueRec bean, get a message from the configured queue. The JMS stuff is configured via the deployment descriptor.
 *
 * <p>Test sync receive for jms ra.
 *
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>
 * @version $Revision: 1.2 $
 */
public class QueueRecBean implements SessionBean {
   
    private final Category log = Category.getInstance(this.getClass());
   
   /**
    * Name used to lookup QueueConnectionFactory
    */
    private static final String CONNECTION_JNDI = "java:comp/env/jms/MyQueueConnection";

   /**
    * Name used to lookup queue destination
    */
    private static final String QUEUE_JNDI = "java:comp/env/jms/QueueName";

    private SessionContext ctx = null;

    private Queue queue = null;
    private QueueConnection queueConnection = null;

    public QueueRecBean() {
    }
    
    public void setSessionContext(SessionContext ctx) {
        this.ctx = ctx;
    }
    
    public void ejbCreate()  {
        try {
            Context context = new InitialContext();

	    // Lookup the queue
            queue = (Queue)context.lookup(QUEUE_JNDI);
	    
	    // Lookup the connection factory
	    QueueConnectionFactory factory = (QueueConnectionFactory)context.lookup(CONNECTION_JNDI);   
	    queueConnection = factory.createQueueConnection();
	    
	    // Keep both around
        } catch (Exception ex) {
            // JMSException or NamingException could be thrown
            log.debug("failed", ex);
	    throw new EJBException(ex.toString());
        }
    }
   
   
    public void ejbRemove() throws RemoteException {
        if(queueConnection != null) {
            try {
	       // Rememer to close the connection when bean is destroyed
                queueConnection.close();
            } catch (Exception e) {
                log.debug("failed", e);
            }
        }
    }

    public void ejbActivate() {}
    public void ejbPassivate() {}

   /**
    * Get a message with sync rec.
    *
    * @return int property name defined in Publisher.JMS_MESSAGE_NR, or -1 if fail.
    */
    public int getMessage() {
	QueueSession   queueSession = null;
	int ret;
	try {
	    
	    // Create a session
            queueSession =
                queueConnection.createQueueSession(true, Session.AUTO_ACKNOWLEDGE);
	    // Get message
	    QueueReceiver queueReceiver = queueSession.createReceiver(queue);
	    Message msg = queueReceiver.receive(500L);
	    if (msg != null)
	    {
	       log.debug("Recived message: " + msg);
	       int nr = msg.getIntProperty(Publisher.JMS_MESSAGE_NR);
	       log.debug("nr: " + nr);
	       ret= nr;
	    }
	    else
	    {
	       log.debug("NO message recived");
	       ret = -1;
	    }
	    	    
        } catch (JMSException ex) {
	   
	   log.debug("failed", ex);
	   ctx.setRollbackOnly();
	   throw new EJBException(ex.toString());
        } finally {
	   // ALWAYS close the session. It's pooled, so do not worry.
	   if (queueSession != null) {
	      try {
		 queueSession.close();
	      } catch (Exception e) {
		 log.debug("failed", e);
	      }
	   }
        }
	return ret;
    }
   
}
