/*
 * @(#)MessageHeadersTopic.java	1.8 00/08/18
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
import java.sql.*;
import java.util.*;
import javax.jms.*;

/**
 * The MessageHeadersTopic class demonstrates the use of message header fields.
 * <p>
 * The program contains a HeaderPublisher class, a HeaderSubscriber class, a
 * display_headers() method that is called by both classes, a main method, and 
 * a method that runs the subscriber and publisher threads.
 * <p>
 * The publishing class sends three messages, and the subscribing class
 * receives them.  The program displays the message headers just before the 
 * publish call and just after the receive so that you can see which ones are
 * set by the publish method.
 * <p>
 * Specify a topic name on the command line when you run the program.  The 
 * program also uses a queue named "controlQueue", which should be created  
 * before you run the program.
 *
 * @author Kim Haase
 * @version 1.8, 08/18/00
 */
public class MessageHeadersTopic {
    final String  CONTROL_QUEUE = "controlQueue";
    String        topicName = null;
    int           exitResult = 0;

    /**
     * The HeaderPublisher class publishes three messages, setting the JMSType  
     * message header field, one of three header fields that are not set by 
     * the publish method.  (The others, JMSCorrelationID and JMSReplyTo, are 
     * demonstrated in the RequestReplyQueue example.)  It also sets a 
     * client property, "messageNumber". 
     *
     * The displayHeaders method is called just before the publish method.
     *
     * @author Kim Haase
     * @version 1.8, 08/18/00
     */
    public class HeaderPublisher extends Thread {

        /**
         * Runs the thread.
         */
        public void run() {
            TopicConnectionFactory  topicConnectionFactory = null;
            TopicConnection         topicConnection = null;
            TopicSession            topicSession = null;
            Topic                   topic = null;
            TopicPublisher          topicPublisher = null;
            TextMessage             message = null;
            final String            MSG_TEXT = new String("Read My Headers");

            try {
                topicConnectionFactory = 
                    SampleUtilities.getTopicConnectionFactory();
                topicConnection = 
                    topicConnectionFactory.createTopicConnection();
                topicSession = topicConnection.createTopicSession(false, 
                    Session.AUTO_ACKNOWLEDGE);
                topic = SampleUtilities.getTopic(topicName, topicSession);
            } catch (Exception e) {
                System.out.println("Connection problem: " + e.toString());
                if (topicConnection != null) {
                    try {
                        topicConnection.close();
                    } catch (JMSException ee) {}
                }
    	        System.exit(1);
            } 
            
            try {
                /*
                 * Synchronize with subscriber.  Wait for message indicating 
                 * that subscriber is ready to receive messages.
                 */
                try {
                    SampleUtilities.receiveSynchronizeMessages("PUBLISHER THREAD: ",
                                                              CONTROL_QUEUE, 1);
                } catch (Exception e) {
                    System.out.println("Queue probably missing: " + e.toString());
                    if (topicConnection != null) {
                        try {
                            topicConnection.close();
                        } catch (JMSException ee) {}
                    }
    	            System.exit(1);
    	        }

                // Create publisher.
                topicPublisher = topicSession.createPublisher(topic);
                
                // First message: no-argument form of publish method
                message = topicSession.createTextMessage();
                message.setJMSType("Simple");
                System.out.println("PUBLISHER THREAD: Setting JMSType to " 
                    + message.getJMSType());
                message.setIntProperty("messageNumber", 1);
                System.out.println("PUBLISHER THREAD: Setting client property messageNumber to " 
                    + message.getIntProperty("messageNumber"));
                message.setText(MSG_TEXT);
                System.out.println("PUBLISHER THREAD: Setting message text to: " 
                    + message.getText());
                System.out.println("PUBLISHER THREAD: Headers before message is sent:");
                displayHeaders(message, "PUBLISHER THREAD: ");
                topicPublisher.publish(message);

                /* 
                 * Second message: 3-argument form of publish method;
                 * explicit setting of delivery mode, priority, and
                 * expiration
                 */
                message = topicSession.createTextMessage();
                message.setJMSType("Less Simple");
                System.out.println("\nPUBLISHER THREAD: Setting JMSType to " 
                    + message.getJMSType());
                message.setIntProperty("messageNumber", 2);
                System.out.println("PUBLISHER THREAD: Setting client property messageNumber to " 
                    + message.getIntProperty("messageNumber"));
                message.setText(MSG_TEXT + " Again");
                System.out.println("PUBLISHER THREAD: Setting message text to: " 
                    + message.getText());
                displayHeaders(message, "PUBLISHER THREAD: ");
                topicPublisher.publish(message, DeliveryMode.NON_PERSISTENT,
                    3, 10000);
                
                /* 
                 * Third message: no-argument form of publish method,
                 * MessageID and Timestamp disabled
                 */
                message = topicSession.createTextMessage();
                message.setJMSType("Disable Test");
                System.out.println("\nPUBLISHER THREAD: Setting JMSType to " 
                    + message.getJMSType());
                message.setIntProperty("messageNumber", 3);
                System.out.println("PUBLISHER THREAD: Setting client property messageNumber to " 
                    + message.getIntProperty("messageNumber"));
                message.setText(MSG_TEXT
                    + " with MessageID and Timestamp disabled");
                System.out.println("PUBLISHER THREAD: Setting message text to: " 
                    + message.getText());
                topicPublisher.setDisableMessageID(true);
                topicPublisher.setDisableMessageTimestamp(true);
                System.out.println("PUBLISHER THREAD: Disabling Message ID and Timestamp");
                displayHeaders(message, "PUBLISHER THREAD: ");
                topicPublisher.publish(message);
            } catch (JMSException e) {
                System.out.println("Exception occurred: " + e.toString());
                exitResult = 1;
            } finally {
                if (topicConnection != null) {
                    try {
                        topicConnection.close();
                    } catch (JMSException e) {
                        exitResult = 1;
                    }
                }
            }
        }
    }
    
    /**
     * The HeaderSubscriber class receives the three messages and calls the
     * displayHeaders method to show how the publish method changed the
     * header values.
     * <p>
     * The first message, in which no fields were set explicitly by the publish 
     * method, shows the default values of these fields.
     * <p>
     * The second message shows the values set explicitly by the publish method.
     * <p>
     * The third message shows whether disabling the MessageID and Timestamp 
     * has any effect in the current JMS implementation.
     *
     * @author Kim Haase
     * @version 1.8, 08/18/00
     */
    public class HeaderSubscriber extends Thread {
 
        /**
         * Runs the thread.
         */
        public void run() {
            TopicConnectionFactory  topicConnectionFactory = null;
            TopicConnection         topicConnection = null;
            TopicSession            topicSession = null;
            Topic                   topic = null;
            TopicSubscriber         topicSubscriber = null;
            final boolean           NOLOCAL = true;
            TextMessage             message = null;

            try {
                topicConnectionFactory =  
                    SampleUtilities.getTopicConnectionFactory();
                topicConnection = 
                    topicConnectionFactory.createTopicConnection();
                topicSession = topicConnection.createTopicSession(false, 
                    Session.AUTO_ACKNOWLEDGE);
                topic = SampleUtilities.getTopic(topicName, topicSession);
            } catch (Exception e) {
                System.out.println("Connection problem: " + e.toString());
                if (topicConnection != null) {
                    try {
                        topicConnection.close();
                    } catch (JMSException ee) {}
                }
    	        System.exit(1);
            } 
            
            /*
             * Create subscriber and start message delivery.
             * Send synchronize message to publisher.
             * Receive the three messages.
             * Call the displayHeaders method to display the message headers.
             */
            try {
                topicSubscriber = 
                    topicSession.createSubscriber(topic, null, NOLOCAL);
                topicConnection.start();

                // Let publisher know that subscriber is ready.
                try {
                    SampleUtilities.sendSynchronizeMessage("SUBSCRIBER THREAD: ",
                                                            CONTROL_QUEUE);
                } catch (Exception e) {
                    System.out.println("Queue probably missing: " + e.toString());
                    if (topicConnection != null) {
                        try {
                            topicConnection.close();
                        } catch (JMSException ee) {}
                    }
    	            System.exit(1);
    	        }

                for (int i = 0; i < 3; i++) {
                    message = (TextMessage) topicSubscriber.receive();
                    System.out.println("\nSUBSCRIBER THREAD: Message received: " 
                        + message.getText());
                    System.out.println("SUBSCRIBER THREAD: Headers after message is received:");
                    displayHeaders(message, "SUBSCRIBER THREAD: ");
                }
            } catch (JMSException e) {
                System.out.println("Exception occurred: " + e.toString());
                exitResult = 1;
            } finally {
                if (topicConnection != null) {
                    try {
                        topicConnection.close();
                    } catch (JMSException e) {
                        exitResult = 1;
                    }
                }
            }   	    
        }
    }
    
    /**
     * Displays all message headers.  Each display is in a try/catch block in
     * case the header is not set before the message is published.
     *
     * @param message	the message whose headers are to be displayed
     * @param prefix	the prefix (publisher or subscriber) to be displayed
     */
    public void displayHeaders (Message message, String prefix) {
        Destination  dest = null;      
        int          delMode = 0;
        long         expiration = 0;
        Time         expTime = null;
        int          priority = 0;
        String       msgID = null;
        long         timestamp = 0;
        Time         timestampTime = null;
        String       correlID = null;
        Destination  replyTo = null;
        boolean      redelivered = false;
        String       type = null;
        String       propertyName = null;
        
        try {
            System.out.println(prefix + "Headers set by publish/send method: ");
            
            // Display the destination (topic, in this case).
            try {
                dest = message.getJMSDestination();
                System.out.println(prefix + " JMSDestination: " + dest);
            } catch (Exception e) {
                System.out.println(prefix + "Exception occurred: " 
                    + e.toString());
                exitResult = 1;
            }
            
            // Display the delivery mode.
            try {
                delMode = message.getJMSDeliveryMode();
                System.out.print(prefix);
                if (delMode == DeliveryMode.NON_PERSISTENT) {
                    System.out.println(" JMSDeliveryMode: non-persistent");
                } else if (delMode == DeliveryMode.PERSISTENT) {
                    System.out.println(" JMSDeliveryMode: persistent");
                } else {
                    System.out.println(" JMSDeliveryMode: neither persistent nor non-persistent; error");
                }
            } catch (Exception e) {
                System.out.println(prefix + "Exception occurred: " 
                    + e.toString());
                exitResult = 1;
            }
            
            /*
             * Display the expiration time.  If value is 0 (the default),
             * the message never expires.  Otherwise, cast the value
             * to a Time object for display.
             */
            try {
                expiration = message.getJMSExpiration();
                System.out.print(prefix);
                if (expiration != 0) {
                    expTime = new Time(expiration);
                    System.out.println(" JMSExpiration: " + expTime);
                } else {
                    System.out.println(" JMSExpiration: " + expiration);
                }
            } catch (Exception e) {
                System.out.println(prefix + "Exception occurred: " 
                    + e.toString());
                exitResult = 1;
            }
            
            // Display the priority.
            try {
                priority = message.getJMSPriority();
                System.out.println(prefix + " JMSPriority: " + priority);
            } catch (Exception e) {
                System.out.println(prefix + "Exception occurred: " 
                    + e.toString());
                exitResult = 1;
            }
            
            // Display the message ID.
            try {
                msgID = message.getJMSMessageID();
                System.out.println(prefix + " JMSMessageID: " + msgID);
            } catch (Exception e) {
                System.out.println(prefix + "Exception occurred: " 
                    + e.toString());
                exitResult = 1;
            }
            
            /*
             * Display the timestamp.
             * If value is not 0, cast it to a Time object for display.
             */
            try {
                timestamp = message.getJMSTimestamp();
                System.out.print(prefix);
                if (timestamp != 0) {
                    timestampTime = new Time(timestamp);
                    System.out.println(" JMSTimestamp: " + timestampTime);
                } else {
                    System.out.println(" JMSTimestamp: " + timestamp);
                }
            } catch (Exception e) {
                System.out.println(prefix + "Exception occurred: " 
                    + e.toString());
                exitResult = 1;
            }
            
            // Display the correlation ID.
            try {
                correlID = message.getJMSCorrelationID();
                System.out.println(prefix + " JMSCorrelationID: " + correlID);
            } catch (Exception e) {
                System.out.println(prefix + "Exception occurred: " 
                    + e.toString());
                exitResult = 1;
            }
            
           // Display the ReplyTo destination.
           try {
                replyTo = message.getJMSReplyTo();
                System.out.println(prefix + " JMSReplyTo: " + replyTo);
            } catch (Exception e) {
                System.out.println(prefix + "Exception occurred: " 
                    + e.toString());
                exitResult = 1;
            }
            
            // Display the Redelivered value (usually false).
            System.out.println(prefix + "Header set by JMS provider:");
            try {
                redelivered = message.getJMSRedelivered();
                System.out.println(prefix + " JMSRedelivered: " + redelivered);
            } catch (Exception e) {
                System.out.println(prefix + "Exception occurred: " 
                    + e.toString());
                exitResult = 1;
            }
            
            // Display the JMSType.
            System.out.println(prefix + "Headers set by client program:");
            try {
                type = message.getJMSType();
                System.out.println(prefix + " JMSType: " + type);
            } catch (Exception e) {
                System.out.println(prefix + "Exception occurred: " 
                    + e.toString());
                exitResult = 1;
            }
            
            // Display any client properties.
            try {
                for (Enumeration e = message.getPropertyNames(); e.hasMoreElements() ;) {
                    propertyName = new String((String) e.nextElement());
                    System.out.println(prefix + " Client property " 
                        + propertyName + ": " 
                        + message.getObjectProperty(propertyName)); 
                }
            } catch (Exception e) {
                System.out.println(prefix + "Exception occurred: " 
                    + e.toString());
                exitResult = 1;
            }
        } catch (Exception e) {
                System.out.println(prefix + "Exception occurred: " 
                    + e.toString());
            exitResult = 1;
        }
    }

    /**
     * Instantiates the subscriber and publisher classes and starts their
     * threads.
     * Calls the join method to wait for the threads to die.
     * <p>
     * It is essential to start the subscriber before starting the publisher.
     * In the publish/subscribe model, a subscriber can ordinarily receive only 
     * messages published while it is active.
     */
    public void run_threads() {
        HeaderSubscriber  headerSubscriber = new HeaderSubscriber();
        HeaderPublisher   headerPublisher = new HeaderPublisher();

        headerSubscriber.start();
        headerPublisher.start();
        try {
            headerSubscriber.join();
            headerPublisher.join();
        } catch (InterruptedException e) {}
    }
    
    /**
     * Reads the topic name from the command line, then calls the
     * run_threads method to execute the program threads.
     *
     * @param args	the topic used by the example
     */
    public static void main(String[] args) {
        MessageHeadersTopic  mht = new MessageHeadersTopic();

        if (args.length != 1) {
    	    System.out.println("Usage: java MessageHeadersTopic <topic_name>");
    	    System.exit(1);
    	}
        mht.topicName = new String(args[0]);
        System.out.println("Topic name is " + mht.topicName);

    	mht.run_threads();
    	SampleUtilities.exit(mht.exitResult); 
    }
}

