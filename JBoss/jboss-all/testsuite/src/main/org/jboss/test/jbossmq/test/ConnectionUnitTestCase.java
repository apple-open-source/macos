/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.test;

import junit.framework.Test;
import junit.framework.TestCase;
import javax.jms.*;
import org.jboss.mq.SpyConnectionFactory;
import org.jboss.test.JBossTestCase;
import java.util.Properties;
import org.jboss.mq.il.oil.OILServerILFactory;
import org.jboss.mq.il.oil2.OIL2ServerILFactory;
import org.jboss.mq.il.uil.UILServerILFactory;
import org.jboss.mq.il.http.HTTPServerILFactory;
import org.jboss.mq.SpyXAConnectionFactory;
import javax.naming.*;
import org.jboss.test.util.AppCallbackHandler;
import javax.security.auth.login.LoginContext;

/** 
 * Test all the verious ways that a connection can be 
 * established with JBossMQ
 *
 * @author hiram.chirino@jboss.org
 * @version $Revision: 1.4.2.2 $
 */
public class ConnectionUnitTestCase extends JBossTestCase {

   public ConnectionUnitTestCase(String name) {
      super(name);
   }

   protected void setUp() throws Exception {
   }

   public void testMultipleRMIConnectViaJNDI() throws Exception {
      
      getLog().debug("Starting testMultipleRMIConnectViaJNDI");

      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("RMIConnectionFactory");

		QueueConnection connections[] = new QueueConnection[10];
		
      getLog().debug("Creating "+connections.length+" connections.");
     for( int i=0; i < connections.length; i++ ) {
	     connections[i] = cf.createQueueConnection();
      }   

      getLog().debug("Closing the connections.");
     for( int i=0; i < connections.length; i++ ) {
	     connections[i].close();
      }   
      
      getLog().debug("Finished testMultipleRMIConnectViaJNDI");
   }

   public void testRMIConnectViaJNDI() throws Exception {
      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("RMIConnectionFactory");
      QueueConnection c = cf.createQueueConnection();
      c.close();

      XAQueueConnectionFactory xacf = (XAQueueConnectionFactory) ctx.lookup("RMIXAConnectionFactory");
      XAQueueConnection xac = xacf.createXAQueueConnection();
      xac.close();
   }

   public void testMultipleOILConnectViaJNDI() throws Exception {
      
      getLog().debug("Starting testMultipleOILConnectViaJNDI");

      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("ConnectionFactory");

		QueueConnection connections[] = new QueueConnection[10];
		
      getLog().debug("Creating "+connections.length+" connections.");
     for( int i=0; i < connections.length; i++ ) {
	     connections[i] = cf.createQueueConnection();
      }   

      getLog().debug("Closing the connections.");
     for( int i=0; i < connections.length; i++ ) {
	     connections[i].close();
      }   
      
      getLog().debug("Finished testMultipleOILConnectViaJNDI");
   }

   public void testOILConnectViaJNDI() throws Exception {
      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("ConnectionFactory");
      QueueConnection c = cf.createQueueConnection();
      c.close();

      XAQueueConnectionFactory xacf = (XAQueueConnectionFactory) ctx.lookup("XAConnectionFactory");
      XAQueueConnection xac = xacf.createXAQueueConnection();
      xac.close();
   }

   public void testOILConnectNoJNDI() throws Exception {

      Properties props = new Properties();
      props.setProperty(OILServerILFactory.SERVER_IL_FACTORY_KEY, OILServerILFactory.SERVER_IL_FACTORY);
      props.setProperty(OILServerILFactory.CLIENT_IL_SERVICE_KEY, OILServerILFactory.CLIENT_IL_SERVICE);
      props.setProperty(OILServerILFactory.PING_PERIOD_KEY, "1000");
      props.setProperty(OILServerILFactory.OIL_ADDRESS_KEY, "localhost");
      props.setProperty(OILServerILFactory.OIL_PORT_KEY, "8090");

      QueueConnectionFactory cf = new SpyConnectionFactory(props);
      QueueConnection c = cf.createQueueConnection();
      c.close();

      XAQueueConnectionFactory xacf = new SpyXAConnectionFactory(props);
      XAQueueConnection xac = xacf.createXAQueueConnection();
      xac.close();

   }

   public void testMultipleOIL2ConnectViaJNDI() throws Exception {
      
      getLog().debug("Starting testMultipleOIL2ConnectViaJNDI");

      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("OIL2ConnectionFactory");

		QueueConnection connections[] = new QueueConnection[10];
		
      getLog().debug("Creating "+connections.length+" connections.");
     for( int i=0; i < connections.length; i++ ) {
	     connections[i] = cf.createQueueConnection();
      }   

      getLog().debug("Closing the connections.");
     for( int i=0; i < connections.length; i++ ) {
	     connections[i].close();
      }   
      
      getLog().debug("Finished testMultipleOIL2ConnectViaJNDI");
   }
   
   public void testOIL2ConnectViaJNDI() throws Exception {
      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("OIL2ConnectionFactory");
      QueueConnection c = cf.createQueueConnection();
      c.close();

      XAQueueConnectionFactory xacf = (XAQueueConnectionFactory) ctx.lookup("OIL2XAConnectionFactory");
      XAQueueConnection xac = xacf.createXAQueueConnection();
      xac.close();
   }

   public void testOIL2ConnectNoJNDI() throws Exception {

      Properties props = new Properties();
      props.setProperty(OIL2ServerILFactory.SERVER_IL_FACTORY_KEY, OIL2ServerILFactory.SERVER_IL_FACTORY);
      props.setProperty(OIL2ServerILFactory.CLIENT_IL_SERVICE_KEY, OIL2ServerILFactory.CLIENT_IL_SERVICE);
      props.setProperty(OIL2ServerILFactory.PING_PERIOD_KEY, "1000");
      props.setProperty(OIL2ServerILFactory.OIL2_ADDRESS_KEY, "localhost");
      props.setProperty(OIL2ServerILFactory.OIL2_PORT_KEY, "8092");

      QueueConnectionFactory cf = new SpyConnectionFactory(props);
      QueueConnection c = cf.createQueueConnection();
      c.close();

      XAQueueConnectionFactory xacf = new SpyXAConnectionFactory(props);
      XAQueueConnection xac = xacf.createXAQueueConnection();
      xac.close();

   }

   public void testMultipleUILConnectViaJNDI() throws Exception {
      
      getLog().debug("Starting testMultipleUILConnectViaJNDI");

      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("UILConnectionFactory");

		QueueConnection connections[] = new QueueConnection[10];
		
      getLog().debug("Creating "+connections.length+" connections.");
     for( int i=0; i < connections.length; i++ ) {
	     connections[i] = cf.createQueueConnection();
      }   

      getLog().debug("Closing the connections.");
     for( int i=0; i < connections.length; i++ ) {
	     connections[i].close();
      }   
      
      getLog().debug("Finished testMultipleUILConnectViaJNDI");
   }

   public void testUILConnectViaJNDI() throws Exception {
      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("UILConnectionFactory");
      QueueConnection c = cf.createQueueConnection();
      c.close();

      XAQueueConnectionFactory xacf = (XAQueueConnectionFactory) ctx.lookup("UILXAConnectionFactory");
      XAQueueConnection xac = xacf.createXAQueueConnection();
      xac.close();
   }

   public void testUILConnectNoJNDI() throws Exception {

      Properties props = new Properties();
      props.setProperty(UILServerILFactory.SERVER_IL_FACTORY_KEY, UILServerILFactory.SERVER_IL_FACTORY);
      props.setProperty(UILServerILFactory.CLIENT_IL_SERVICE_KEY, UILServerILFactory.CLIENT_IL_SERVICE);
      props.setProperty(UILServerILFactory.PING_PERIOD_KEY, "1000");
      props.setProperty(UILServerILFactory.UIL_ADDRESS_KEY, "localhost");
      props.setProperty(UILServerILFactory.UIL_PORT_KEY, "8091");

      QueueConnectionFactory cf = new SpyConnectionFactory(props);
      QueueConnection c = cf.createQueueConnection();
      c.close();

      XAQueueConnectionFactory xacf = new SpyXAConnectionFactory(props);
      XAQueueConnection xac = xacf.createXAQueueConnection();
      xac.close();
   }

   public void testMultipleUIL2ConnectViaJNDI() throws Exception {
      
      getLog().debug("Starting testMultipleUIL2ConnectViaJNDI");

      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("UIL2ConnectionFactory");

		QueueConnection connections[] = new QueueConnection[10];
		
      getLog().debug("Creating "+connections.length+" connections.");
     for( int i=0; i < connections.length; i++ ) {
	     connections[i] = cf.createQueueConnection();
      }   

      getLog().debug("Closing the connections.");
     for( int i=0; i < connections.length; i++ ) {
	     connections[i].close();
      }   
      
      getLog().debug("Finished testMultipleUIL2ConnectViaJNDI");
   }

   public void testUIL2ConnectViaJNDI() throws Exception {
      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("UIL2ConnectionFactory");
      QueueConnection c = cf.createQueueConnection();
      c.close();

      XAQueueConnectionFactory xacf = (XAQueueConnectionFactory) ctx.lookup("UIL2XAConnectionFactory");
      XAQueueConnection xac = xacf.createXAQueueConnection();
      xac.close();
   }

   public void testUIL2ConnectNoJNDI() throws Exception {

      Properties props = new Properties();
      props.setProperty(org.jboss.mq.il.uil2.UILServerILFactory.SERVER_IL_FACTORY_KEY, 
                        org.jboss.mq.il.uil2.UILServerILFactory.SERVER_IL_FACTORY);
      props.setProperty(org.jboss.mq.il.uil2.UILServerILFactory.CLIENT_IL_SERVICE_KEY, 
                        org.jboss.mq.il.uil2.UILServerILFactory.CLIENT_IL_SERVICE);
      props.setProperty(org.jboss.mq.il.uil2.UILServerILFactory.PING_PERIOD_KEY, "1000");
      props.setProperty(org.jboss.mq.il.uil2.UILServerILFactory.UIL_ADDRESS_KEY, "localhost");
      props.setProperty(org.jboss.mq.il.uil2.UILServerILFactory.UIL_PORT_KEY, "8093");

      QueueConnectionFactory cf = new SpyConnectionFactory(props);
      QueueConnection c = cf.createQueueConnection();
      c.close();

      XAQueueConnectionFactory xacf = new SpyXAConnectionFactory(props);
      XAQueueConnection xac = xacf.createXAQueueConnection();
      xac.close();
   }

   public void testMultipleHTTPConnectViaJNDI() throws Exception {
      
      getLog().debug("Starting testMultipleHTTPConnectViaJNDI");

      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("HTTPConnectionFactory");

		QueueConnection connections[] = new QueueConnection[10];
		
      getLog().debug("Creating "+connections.length+" connections.");
     for( int i=0; i < connections.length; i++ ) {
	     connections[i] = cf.createQueueConnection();
      }   

      getLog().debug("Closing the connections.");
     for( int i=0; i < connections.length; i++ ) {
	     connections[i].close();
      }   
      
      getLog().debug("Finished testMultipleHTTPConnectViaJNDI");
   }
   
   public void testHTTPConnectViaJNDI() throws Exception {
      InitialContext ctx = new InitialContext();

      QueueConnectionFactory cf = (QueueConnectionFactory) ctx.lookup("HTTPConnectionFactory");
      QueueConnection c = cf.createQueueConnection();
      c.close();

      XAQueueConnectionFactory xacf = (XAQueueConnectionFactory) ctx.lookup("HTTPXAConnectionFactory");
      XAQueueConnection xac = xacf.createXAQueueConnection();
      xac.close();
   }
   
   public void testHTTPConnectNoJNDI() throws Exception {
        
        Properties props = new Properties();
        props.setProperty(HTTPServerILFactory.SERVER_IL_FACTORY_KEY, HTTPServerILFactory.SERVER_IL_FACTORY);
        props.setProperty(HTTPServerILFactory.CLIENT_IL_SERVICE_KEY, HTTPServerILFactory.CLIENT_IL_SERVICE);
        props.setProperty(HTTPServerILFactory.SERVER_URL_KEY, "http://localhost:8080/jbossmq-httpil/HTTPServerILServlet");
        props.setProperty(HTTPServerILFactory.PING_PERIOD_KEY, "0");
        props.setProperty(HTTPServerILFactory.TIMEOUT_KEY, "60");
        props.setProperty(HTTPServerILFactory.REST_INTERVAL_KEY, "1");
        
        QueueConnectionFactory cf = new SpyConnectionFactory(props);
        QueueConnection c = cf.createQueueConnection();
        c.close();
        
        XAQueueConnectionFactory xacf = new SpyXAConnectionFactory(props);
        XAQueueConnection xac = xacf.createXAQueueConnection();
        xac.close();
   }
   
   public void testHTTPConnectNoJNDIWithBasicAuthentication() throws Exception {

        ////////////////////THIS IS HOW YOU HANDLE SECURITY ////////////////////
        String authConf = super.getResourceURL("security/auth.conf");
        System.setProperty("java.security.auth.login.config", authConf);
        AppCallbackHandler handler = new AppCallbackHandler("httpil", "httpil".toCharArray());
        LoginContext lc = new LoginContext("other", handler);
        lc.login();
        ////////////////////////////////////////////////////////////////////////
        
        Properties props = new Properties();
        props.setProperty(HTTPServerILFactory.SERVER_IL_FACTORY_KEY, HTTPServerILFactory.SERVER_IL_FACTORY);
        props.setProperty(HTTPServerILFactory.CLIENT_IL_SERVICE_KEY, HTTPServerILFactory.CLIENT_IL_SERVICE);
        props.setProperty(HTTPServerILFactory.SERVER_URL_KEY, "http://localhost:8080/jbossmq-httpil/restricted/HTTPServerILServlet");
        props.setProperty(HTTPServerILFactory.PING_PERIOD_KEY, "0");
        props.setProperty(HTTPServerILFactory.TIMEOUT_KEY, "60");
        props.setProperty(HTTPServerILFactory.REST_INTERVAL_KEY, "1");
        
        QueueConnectionFactory cf = new SpyConnectionFactory(props);
        QueueConnection c = cf.createQueueConnection();
        c.close();
        
        XAQueueConnectionFactory xacf = new SpyXAConnectionFactory(props);
        XAQueueConnection xac = xacf.createXAQueueConnection();
        xac.close();
        lc.logout();    // Log out.
   }

   public static void main(java.lang.String[] args) {
      junit.textui.TestRunner.run(ConnectionUnitTestCase.class);
   }
}