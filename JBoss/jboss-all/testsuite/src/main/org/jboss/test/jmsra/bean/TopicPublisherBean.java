package org.jboss.test.jmsra.bean;

import java.rmi.RemoteException;
import java.util.*;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.ejb.EJBException;
import javax.naming.*;
import javax.jms.*;


public class TopicPublisherBean implements SessionBean {

       static org.apache.log4j.Category log =
       org.apache.log4j.Category.getInstance(TopicPublisherBean.class);

    private static final String CONNECTION_JNDI = "java:comp/env/jms/MyTopicConnection";
    private static final String TOPIC_JNDI = "java:comp/env/jms/TopicName";

    private static final String BEAN_JNDI = "java:comp/env/ejb/PublisherCMP";

    private SessionContext ctx = null;
    private Topic topic = null;
    private TopicConnection topicConnection = null;
    public TopicPublisherBean() {
    }

    public void setSessionContext(SessionContext ctx) {
        this.ctx = ctx;
    }

    public void ejbCreate()  {
        try {
            Context context = new InitialContext();
            topic = (Topic)context.lookup(TOPIC_JNDI);
    
        TopicConnectionFactory factory = (TopicConnectionFactory)context.lookup(CONNECTION_JNDI);   
        topicConnection = factory.createTopicConnection();
    
        } catch (Exception ex) {
            // JMSException or NamingException could be thrown
            log.debug("failed", ex);
        throw new EJBException(ex.toString());
        }
    }

    /**
     * Send a message with a message nr in property MESSAGE_NR
     */
    public void simple(int messageNr)  {
    sendMessage(messageNr);
    }
    /**
     * Try send a message with a message nr in property MESSAGE_NR,
     * but set rollback only
     */
    public void simpleFail(int messageNr)  {
    sendMessage(messageNr);
    // Roll it back, no message should be sent if transactins work
    log.debug("DEBUG: Setting rollbackOnly");
    ctx.setRollbackOnly();
    log.debug("DEBUG rollback set: " + ctx.getRollbackOnly());
    
    }

    public void beanOk(int messageNr) {
    // DO JMS - First transaction
    sendMessage(messageNr);
    PublisherCMPHome h = null;
    try {
        // DO entity bean - Second transaction
        h = (PublisherCMPHome) new InitialContext().lookup(BEAN_JNDI);
        PublisherCMP b = h.create(new Integer(messageNr));
        b.ok(messageNr);
    }catch(Exception ex) {
        throw new EJBException("OPS exception in ok: " + ex);
    } finally {
        try {
        h.remove(new Integer(messageNr));
        }catch(Exception e) {
        log.debug("failed", e);
        }
    }
    
    }

    public void beanError(int messageNr) {
    // DO JMS - First transaction
    sendMessage(messageNr);
    PublisherCMPHome h = null;
    try {
        // DO entity bean - Second transaction
        h = (PublisherCMPHome) new InitialContext().lookup(BEAN_JNDI);
        PublisherCMP b = h.create(new Integer(messageNr));
        b.error(messageNr);
    } catch(Exception ex) {
        throw new EJBException("Exception in erro: " + ex);
    }finally {
        try {
        h.remove(new Integer(messageNr));
        }catch(Exception ex) {
        log.debug("failed", ex);
        }
    }
    }

    public void ejbRemove() throws RemoteException {
        if(topicConnection != null) {
            try {
                topicConnection.close();
            } catch (Exception e) {
                log.debug("failed", e);
            }
        }
    }

    public void ejbActivate() {}
    public void ejbPassivate() {}

    private void sendMessage(int messageNr) {
    TopicSession   topicSession = null;
    try {
        TopicPublisher topicPublisher = null;
        TextMessage    message = null;
    
            topicSession =
                topicConnection.createTopicSession(true, Session.AUTO_ACKNOWLEDGE);
            topicPublisher = topicSession.createPublisher(topic);
    
            message = topicSession.createTextMessage();
        message.setText(String.valueOf(messageNr));
        message.setIntProperty(Publisher.JMS_MESSAGE_NR, messageNr);
        topicPublisher.publish(message);
    
    
        } catch (JMSException ex) {
    
            log.debug("failed", ex);
            ctx.setRollbackOnly();
        throw new EJBException(ex.toString());
        } finally {
            if (topicSession != null) {
                try {
                    topicSession.close();
                } catch (Exception e) {
                    log.debug("failed", e);
                }
            }
        }
    }

}
