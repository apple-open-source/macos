/*
 * @(#)DurableSubscriberExample.java	1.6 00/08/18
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
 * The DurableSubscriberExample class demonstrates that a durable subscription
 * is active even when the subscriber is not active.
 * <p>
 * The program contains a DurableSubscriber class, a MultiplePublisher class, 
 * a main method, and a method that instantiates the classes and calls their
 * methods in sequence.
 * <p>
 * The program begins like any publish/subscribe program: the subscriber starts,
 * the publisher publishes some messages, and the subscriber receives them.
 * <p>
 * At this point the subscriber closes itself.  The publisher then publishes 
 * some messages while the subscriber is not active.  The subscriber then 
 * restarts and receives the messages.
 * <p>
 * Specify a topic name on the command line when you run the program.
 *
 * @author Kim Haase
 * @version 1.6, 08/18/00
 */
public class DurableSubscriberExample {
	String      topicName = null;
	int         exitResult = 0;
	static int  startindex = 0;

	/**
	 * The DurableSubscriber class contains a constructor, a startSubscriber 
	 * method, a closeSubscriber method, and a finish method.
	 * <p>
	 * The class fetches messages asynchronously, using a message listener, 
	 * TextListener.
	 *
	 * @author Kim Haase
	 * @version 1.6, 08/18/00
	 */
	public class DurableSubscriber {
		TopicConnection  topicConnection = null;
		TopicSession     topicSession = null;
		Topic            topic = null;
		TopicSubscriber  topicSubscriber = null;
		TextListener     topicListener = null;

		/**
		 * The TextListener class implements the MessageListener interface by 
		 * defining an onMessage method for the DurableSubscriber class.
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
						System.out.println("SUBSCRIBER: Reading message: " 
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
		 * Constructor: looks up a connection factory and topic and creates a 
		 * connection and session.
		 */
		public DurableSubscriber() {
			TopicConnectionFactory  topicConnectionFactory = null;

			try {
				topicConnectionFactory = 
					SampleUtilities.getTopicConnectionFactory();
				topicConnection = 
					topicConnectionFactory.createTopicConnection("john", "needle");
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
		}

		/**
		 * Stops connection, then creates durable subscriber, registers message 
		 * listener (TextListener), and starts message delivery; listener
		 * displays the messages obtained.
		 */
		public void startSubscriber() {
			try {
				System.out.println("Starting subscriber");
				topicConnection.stop();
				topicSubscriber = topicSession.createDurableSubscriber(topic,
					"MakeItLast");
				topicListener = new TextListener();
				topicSubscriber.setMessageListener(topicListener);
				topicConnection.start();
			} catch (JMSException e) {
				System.out.println("Exception occurred: " + e.toString());
				exitResult = 1;
			}
		}
		
		/**
		 * Blocks until publisher issues a control message indicating
		 * end of publish stream, then closes subscriber.
		 */
		public void closeSubscriber() {
			try {
				topicListener.monitor.waitTillDone();
				System.out.println("Closing subscriber");
				topicSubscriber.close();
			} catch (JMSException e) {
				System.out.println("Exception occurred: " + e.toString());
				exitResult = 1;
			}
		}
		
		/**
		 * Closes the connection.
		 */
		public void finish() {
			if (topicConnection != null) {
				try {
					topicSession.unsubscribe("MakeItLast");
					topicConnection.close();
				} catch (JMSException e) {
					exitResult = 1;
				}
			}
		}
	}

	/**
	 * The MultiplePublisher class publishes several messages to a topic. It
	 * contains a constructor, a publishMessages method, and a finish method.
	 *
	 * @author Kim Haase
	 * @version 1.6, 08/18/00
	 */
	public class MultiplePublisher {
		TopicConnection  topicConnection = null;
		TopicSession     topicSession = null;
		Topic            topic = null;
		TopicPublisher   topicPublisher = null;

		/**
		 * Constructor: looks up a connection factory and topic and creates a 
		 * connection and session.  Also creates the publisher.
		 */
		public MultiplePublisher() {
			TopicConnectionFactory  topicConnectionFactory = null;

			try {
				topicConnectionFactory = 
					SampleUtilities.getTopicConnectionFactory();
				topicConnection = 
					topicConnectionFactory.createTopicConnection();
				topicSession = topicConnection.createTopicSession(false, 
					Session.AUTO_ACKNOWLEDGE);
				topic = SampleUtilities.getTopic(topicName, topicSession);
				topicPublisher = topicSession.createPublisher(topic);
			} catch (Exception e) {
				System.out.println("Connection problem: " + e.toString());
				if (topicConnection != null) {
					try {
						topicConnection.close();
					} catch (JMSException ee) {}
				}
		        System.exit(1);
			} 
		}
		
		/**
		 * Creates text message.
		 * Sends some messages, varying text slightly.
		 * Messages must be persistent.
		 */
		public void publishMessages() {
			TextMessage   message = null;
			int           i;
			final int     NUMMSGS = 3;
			final String  MSG_TEXT = new String("Here is a message");

			try {
				message = topicSession.createTextMessage();
				for (i = startindex; i < startindex + NUMMSGS; i++) {
					message.setText(MSG_TEXT + " " + (i + 1));
					System.out.println("PUBLISHER: Publishing message: " 
						+ message.getText());
					topicPublisher.publish(message);
				}

				// Send a non-text control message indicating end of messages.
				topicPublisher.publish(topicSession.createMessage());
				startindex = i;
			} catch (JMSException e) {
				System.out.println("Exception occurred: " + e.toString());
				exitResult = 1;
			}
		}
		
		/**
		 * Closes the connection.
		 */
		public void finish() {
			if (topicConnection != null) {
				try {
					topicConnection.close();
				} catch (JMSException e) {
					exitResult = 1;
				}
			}
		}
	}
    
	/**
	 * Instantiates the subscriber and publisher classes.
	 *
	 * Starts the subscriber; the publisher publishes some messages.
	 *
	 * Closes the subscriber; while it is closed, the publisher publishes
	 * some more messages.
	 *
	 * Restarts the subscriber and fetches the messages.
	 *
	 * Finally, closes the connections.    
	 */
	public void run_program() {
		DurableSubscriber  durableSubscriber = new DurableSubscriber();
		MultiplePublisher  multiplePublisher = new MultiplePublisher();

		durableSubscriber.startSubscriber();
		multiplePublisher.publishMessages();
		durableSubscriber.closeSubscriber();
		multiplePublisher.publishMessages();
		durableSubscriber.startSubscriber();
		durableSubscriber.closeSubscriber();
		multiplePublisher.finish();
		durableSubscriber.finish();
	}

	/**
	 * Reads the topic name from the command line, then calls the
	 * run_program method.
	 *
	 * @param args	the topic used by the example
	 */
	public static void main(String[] args) {
		DurableSubscriberExample  dse = new DurableSubscriberExample();
		
		if (args.length != 1) {
		    System.out.println("Usage: java DurableSubscriberExample <topic_name>");
		    System.exit(1);
		}
		dse.topicName = new String(args[0]);
		System.out.println("Topic name is " + dse.topicName);

		dse.run_program();
		SampleUtilities.exit(dse.exitResult);
	}
}
