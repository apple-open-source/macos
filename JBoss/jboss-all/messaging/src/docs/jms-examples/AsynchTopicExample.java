/*
 * @(#)AsynchTopicExample.java	1.6 00/08/18
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
import javax.jms.*;

/**
 * The AsynchTopicExample class demonstrates the use of a message listener in 
 * the publish/subscribe model.  The publisher publishes several messages, and
 * the subscriber reads them asynchronously.
 * <p>
 * The program contains a MultiplePublisher class, an AsynchSubscriber class
 * with a listener class, a main method, and a method that runs the subscriber
 * and publisher threads.
 * <p>
 * Specify a topic name on the command line when you run the program.  The 
 * program also uses a queue named "controlQueue", which should be created  
 * before you run the program.
 *
 * @author Kim Haase
 * @version 1.6, 08/18/00
 */
public class AsynchTopicExample {
    final String  CONTROL_QUEUE = "controlQueue";
    String        topicName = null;
    int           exitResult = 0;

    /**
     * The AsynchSubscriber class fetches several messages from a topic 
     * asynchronously, using a message listener, TextListener.
     *
     * @author Kim Haase
     * @version 1.6, 08/18/00
     */
    public class AsynchSubscriber extends Thread {

        /**
         * The TextListener class implements the MessageListener interface by 
         * defining an onMessage method for the AsynchSubscriber class.
         *
         * @author Kim Haase
         * @version 1.6, 08/18/00
         */
        private class TextListener implements MessageListener {
            final SampleUtilities.DoneLatch  monitor =
                new SampleUtilities.DoneLatch();

            /**
             * Casts the message to a TextMessage and displays its text.
             * A non-text message is interpreted as the end of the message 
             * stream, and the message listener sets its monitor state to all 
             * done processing messages.
             *
             * @param message	the incoming message
             */
            public void onMessage(Message message) {
                if (message instanceof TextMessage) {
                    TextMessage  msg = (TextMessage) message;

                    try {
                        System.out.println("SUBSCRIBER THREAD: Reading message: " 
                                           + msg.getText());
                    } catch (JMSException e) {
                        System.out.println("Exception in onMessage(): " 
                                           + e.toString());
                    }
                } else {
                    monitor.allDone();
                }
            }
        }

        /**
         * Runs the thread.
         */
        public void run() {
            TopicConnectionFactory  topicConnectionFactory = null;
            TopicConnection         topicConnection = null;
            TopicSession            topicSession = null;
            Topic                   topic = null;
            TopicSubscriber         topicSubscriber = null;
            TextListener            topicListener = null;

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
             * Create subscriber.
             * Register message listener (TextListener).
             * Start message delivery.
             * Send synchronize message to publisher, then wait till all
             * messages have arrived.
             * Listener displays the messages obtained.
             */
            try {
                topicSubscriber = topicSession.createSubscriber(topic);
                topicListener = new TextListener();
                topicSubscriber.setMessageListener(topicListener);
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

                /*
                 * Asynchronously process messages.
                 * Block until publisher issues a control message indicating
                 * end of publish stream.
                 */
                topicListener.monitor.waitTillDone();
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
     * The MultiplePublisher class publishes several message to a topic. 
     *
     * @author Kim Haase
     * @version 1.6, 08/18/00
     */
    public class MultiplePublisher extends Thread {

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
            final int               NUMMSGS = 20;
            final String            MSG_TEXT = new String("Here is a message");

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
             * After synchronizing with subscriber, create publisher.
             * Create text message.
             * Send messages, varying text slightly.
             * Send end-of-messages message.
             * Finally, close connection.
             */
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
                
                topicPublisher = topicSession.createPublisher(topic);
                message = topicSession.createTextMessage();
                for (int i = 0; i < NUMMSGS; i++) {
                    message.setText(MSG_TEXT + " " + (i + 1));
                    System.out.println("PUBLISHER THREAD: Publishing message: " 
                        + message.getText());
                    topicPublisher.publish(message);
                }

                // Send a non-text control message indicating end of messages.
                topicPublisher.publish(topicSession.createMessage());
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
     * Instantiates the subscriber and publisher classes and starts their
     * threads.
     * Calls the join method to wait for the threads to die.
     * <p>
     * It is essential to start the subscriber before starting the publisher.
     * In the publish/subscribe model, a subscriber can ordinarily receive only 
     * messages published while it is active.
     */
    public void run_threads() {
        AsynchSubscriber   asynchSubscriber = new AsynchSubscriber();
        MultiplePublisher  multiplePublisher = new MultiplePublisher();

        multiplePublisher.start();
        asynchSubscriber.start();
        try {
            asynchSubscriber.join();
            multiplePublisher.join();
        } catch (InterruptedException e) {}
    }

    /**
     * Reads the topic name from the command line, then calls the
     * run_threads method to execute the program threads.
     *
     * @param args	the topic used by the example
     */
    public static void main(String[] args) {
        AsynchTopicExample  ate = new AsynchTopicExample();
        
        if (args.length != 1) {
    	    System.out.println("Usage: java AsynchTopicExample <topic_name>");
    	    System.exit(1);
    	}
        ate.topicName = new String(args[0]);
        System.out.println("Topic name is " + ate.topicName);

    	ate.run_threads();
    	SampleUtilities.exit(ate.exitResult);
    }
}

