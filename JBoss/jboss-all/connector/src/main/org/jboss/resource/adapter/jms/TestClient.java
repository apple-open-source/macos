/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.resource.adapter.jms;

import javax.jms.*;
import javax.naming.InitialContext;

/**
 * TestClient for stand alone use. Basically verry uninteresting.
 *
 * Created: Sun Apr 22 19:10:27 2001
 *
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 * @version $Revision: 1.1 $
 */
public class TestClient
{
   public TestClient() {
      // empty
   }
    
   public static void main(String[] args) {
      try {
         JmsManagedConnectionFactory f = new JmsManagedConnectionFactory();
         f.setJmsProviderAdapter( new org.jboss.jms.jndi.JBossMQProvider());
         //f.setLogging("true");
         JmsConnectionFactory cf = (JmsConnectionFactory)f.createConnectionFactory();
         
         //FIXME - how to get LocalTransaction for standalone usage?
         TopicConnection con = cf.createTopicConnection();
         TopicSession ses = con.createTopicSession(true, Session.AUTO_ACKNOWLEDGE);
         Topic topic = (Topic)new InitialContext().lookup("topic/testTopic");
         
         
         TopicPublisher pub = ses.createPublisher(topic);
         
         TextMessage m = ses.createTextMessage("Hello world!");
         pub.publish(m);
         ses.commit();
         
         ses.close();
      }
      catch(Exception ex) {
         System.err.println("Error: " + ex);
         ex.printStackTrace();
      }
   }
}
