/*
 * @(#)SampleUtilities.java	1.7 00/08/18
 * 
 * Copyright (c) 2000 Sun Microsystems, Inc. All Rights Reserved.
 * 
 * Sun grants you ("Licensee") a non-exclusive, royalty free, license to use,
 * modify and redistribute this software in source and binary code form,
 * provided that i) this copyright notice and license appear on all copies of
 * the software; and ii) Licensee does not utilize the software in a manner
 * which is disparaging to Sun.
 *
 * This software is provided "AS IS," without a warranty of any kind. ALL
 * EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NON-INFRINGEMENT, ARE HEREBY EXCLUDED. SUN AND ITS LICENSORS SHALL NOT BE
 * LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THE SOFTWARE OR ITS DERIVATIVES. IN NO EVENT WILL SUN OR ITS
 * LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA, OR FOR DIRECT,
 * INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER
 * CAUSED AND REGARDLESS OF THE THEORY OF LIABILITY, ARISING OUT OF THE USE OF
 * OR INABILITY TO USE SOFTWARE, EVEN IF SUN HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * This software is not designed or intended for use in on-line control of
 * aircraft, air traffic, aircraft navigation or aircraft communications; or in
 * the design, construction, operation or maintenance of any nuclear
 * facility. Licensee represents and warrants that it will not use or
 * redistribute the Software for such purposes.
 */
import javax.naming.*;
import javax.jms.*;

/**
 * Utility class for JMS sample programs.
 * <p>
 * Set the <code>USE_JNDI</code> variable to true or false depending on whether
 * your provider uses JNDI. 
 * <p>
 * Contains the following methods: 
 * <ul> 
 *   <li> getQueueConnectionFactory
 *   <li> getTopicConnectionFactory
 *   <li> getQueue
 *   <li> getTopic
 *   <li> jndiLookup
 *   <li> exit
 *   <li> receiveSynchronizeMessages
 *   <li> sendSynchronizeMessages
 * </ul>
 *
 * Also contains the class DoneLatch, which contains the following methods:
 * <ul> 
 *   <li> waitTillDone
 *   <li> allDone
 * </ul>
 *
 * @author Kim Haase
 * @author Joseph Fialli
 * @version 1.7, 08/18/00
 */
public class SampleUtilities {
    public static final boolean USE_JNDI = true;
    public static final String  QUEUECONFAC = "ConnectionFactory";
    public static final String  TOPICCONFAC = "ConnectionFactory";
    private static Context      jndiContext = null;
    
    /**
     * Returns a QueueConnectionFactory object.
     * If provider uses JNDI, serves as a wrapper around jndiLookup method. 
     * If provider does not use JNDI, substitute provider-specific code here.
     *
     * @return		a QueueConnectionFactory object
     * @throws		javax.naming.NamingException (or other exception)
     *                   if name cannot be found
     */
    public static javax.jms.QueueConnectionFactory getQueueConnectionFactory() 
      throws Exception {
        if (USE_JNDI) {
            return (javax.jms.QueueConnectionFactory) jndiLookup(QUEUECONFAC);
        } else {
            // return new provider-specific QueueConnectionFactory
            return null;
        }
    }
    
    /**
     * Returns a TopicConnectionFactory object.
     * If provider uses JNDI, serves as a wrapper around jndiLookup method.
     * If provider does not use JNDI, substitute provider-specific code here.
     *
     * @return		a TopicConnectionFactory object
     * @throws		javax.naming.NamingException (or other exception)
     *                   if name cannot be found
     */
    public static javax.jms.TopicConnectionFactory getTopicConnectionFactory() 
      throws Exception {
        if (USE_JNDI) {
            return (javax.jms.TopicConnectionFactory) jndiLookup(TOPICCONFAC);
        } else {
            // return new provider-specific TopicConnectionFactory
            return null;
        }
    }
    
    /**
     * Returns a Queue object.
     * If provider uses JNDI, serves as a wrapper around jndiLookup method.
     * If provider does not use JNDI, substitute provider-specific code here.
     *
     * @param name      String specifying queue name
     * @param session   a QueueSession object
     *
     * @return		a Queue object
     * @throws		javax.naming.NamingException (or other exception)
     *                   if name cannot be found
     */
    public static javax.jms.Queue getQueue(String name, 
                                           javax.jms.QueueSession session) 
      throws Exception {
        if (USE_JNDI) {
            return (javax.jms.Queue) jndiLookup("queue/"+name);
        } else {
            return session.createQueue(name);
        }
    }
    
    /**
     * Returns a Topic object.
     * If provider uses JNDI, serves as a wrapper around jndiLookup method.
     * If provider does not use JNDI, substitute provider-specific code here.
     *
     * @param name      String specifying topic name
     * @param session   a TopicSession object
     *
     * @return		a Topic object
     * @throws		javax.naming.NamingException (or other exception)
     *                   if name cannot be found
     */
    public static javax.jms.Topic getTopic(String name, 
                                           javax.jms.TopicSession session) 
      throws Exception {
        if (USE_JNDI) {
            return (javax.jms.Topic) jndiLookup("topic/"+name);
        } else {
            return session.createTopic(name);
        }
    }
    
    /**
     * Creates a JNDI InitialContext object if none exists yet.  Then looks up 
     * the string argument and returns the associated object.
     *
     * @param name	the name of the object to be looked up
     *
     * @return		the object bound to <code>name</code>
     * @throws		javax.naming.NamingException if name cannot be found
     */
    public static Object jndiLookup(String name) throws NamingException {
        Object    obj = null;

        if (jndiContext == null) {
            try {
                jndiContext = new InitialContext();
            } catch (NamingException e) {
                System.out.println("Could not create JNDI context: " + 
                    e.toString());
                throw e;
            }
        }
        try {
           obj = jndiContext.lookup(name);
        } catch (NamingException e) {
            System.out.println("JNDI lookup failed: " + e.toString());
            throw e;
        }
        return obj;
    }
   
    /**
     * Calls System.exit().
     * 
     * @param result	The exit result; 0 indicates no errors
     */
    public static void exit(int result) {
        System.exit(result);
    }
   
    /**
     * Wait for 'count' messages on controlQueue before continuing.  Called by
     * a publisher to make sure that subscribers have started before it begins
     * publishing messages.
     * <p>
     * If controlQueue doesn't exist, the method throws an exception.
     *
     * @param prefix	prefix (publisher or subscriber) to be displayed
     * @param controlQueueName	name of control queue 
     * @param count	number of messages to receive
     */
    public static void receiveSynchronizeMessages(String prefix,
                                                  String controlQueueName, 
                                                  int count) 
      throws Exception {
        QueueConnectionFactory  queueConnectionFactory = null;
        QueueConnection         queueConnection = null;
        QueueSession            queueSession = null;
        Queue                   controlQueue = null;
        QueueReceiver           queueReceiver = null;

        try {
            queueConnectionFactory = 
                SampleUtilities.getQueueConnectionFactory();
            queueConnection = queueConnectionFactory.createQueueConnection();
            queueSession = queueConnection.createQueueSession(false, 
                                                 Session.AUTO_ACKNOWLEDGE);
            controlQueue = getQueue(controlQueueName, queueSession);
            queueConnection.start();
        } catch (Exception e) {
            System.out.println("Connection problem: " + e.toString());
            if (queueConnection != null) {
                try {
                    queueConnection.close();
                } catch (JMSException ee) {}
            }
            throw e;
        } 

        try {
            System.out.println(prefix + "Receiving synchronize messages from "
                               + controlQueueName + "; count = " + count);
            queueReceiver = queueSession.createReceiver(controlQueue);
            while (count > 0) {
                queueReceiver.receive();
                count--;
                System.out.println(prefix 
                                   + "Received synchronize message; expect " 
                                   + count + " more");
            }
        } catch (JMSException e) {
            System.out.println("Exception occurred: " + e.toString());
            throw e;
        } finally {
            if (queueConnection != null) {
                try {
                    queueConnection.close();
                } catch (JMSException e) {}
            }
        }
    }

    /**
     * Send a message to controlQueue.  Called by a subscriber to notify a
     * publisher that it is ready to receive messages.
     * <p>
     * If controlQueue doesn't exist, the method throws an exception.
     *
     * @param prefix	prefix (publisher or subscriber) to be displayed
     * @param controlQueueName	name of control queue
     */
    public static void sendSynchronizeMessage(String prefix,
                                              String controlQueueName) 
      throws Exception {
        QueueConnectionFactory  queueConnectionFactory = null;
        QueueConnection         queueConnection = null;
        QueueSession            queueSession = null;
        Queue                   controlQueue = null;
        QueueSender             queueSender = null;
        TextMessage             message = null;

        try {
            queueConnectionFactory = 
                SampleUtilities.getQueueConnectionFactory();
            queueConnection = queueConnectionFactory.createQueueConnection();
            queueSession = queueConnection.createQueueSession(false,
                                                 Session.AUTO_ACKNOWLEDGE);
            controlQueue = getQueue(controlQueueName, queueSession);
        } catch (Exception e) {
            System.out.println("Connection problem: " + e.toString());
            if (queueConnection != null) {
                try {
                    queueConnection.close();
                } catch (JMSException ee) {}
            }
            throw e;
        } 

        try {
            queueSender = queueSession.createSender(controlQueue);
            message = queueSession.createTextMessage();
            message.setText("synchronize");
            System.out.println(prefix + "Sending synchronize message to " 
                               + controlQueueName);
            queueSender.send(message);
        } catch (JMSException e) {
            System.out.println("Exception occurred: " + e.toString());
            throw e;
        } finally {
            if (queueConnection != null) {
                try {
                    queueConnection.close();
                } catch (JMSException e) {}
            }
        }
    }

    /**
     * Monitor class for asynchronous examples.  Producer signals end of
     * message stream; listener calls allDone() to notify consumer that the 
     * signal has arrived, while consumer calls waitTillDone() to wait for this 
     * notification.
     *
     * @author Joseph Fialli
     * @version 1.7, 08/18/00
     */
    static public class DoneLatch {
        boolean  done = false;

        /**
         * Waits until done is set to true.
         */
        public void waitTillDone() {
            synchronized (this) {
                while (! done) {
                    try {
                        this.wait();
                    } catch (InterruptedException ie) {}
                }
            }
        }
        
        /**
         * Sets done to true.
         */
        public void allDone() {
            synchronized (this) {
                done = true;
                this.notify();
            }
        }
    }
}

