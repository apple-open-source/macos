/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.web.test;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;
import javax.management.ObjectName;

import junit.framework.Test;
import junit.framework.TestSuite;

import org.apache.commons.httpclient.HttpClient;
import org.apache.commons.httpclient.UsernamePasswordCredentials;
import org.apache.commons.httpclient.methods.GetMethod;

import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSetup;

/** Tests of servlet container integration into the JBoss server. This test
 requires than a web container be integrated into the JBoss server. The tests
 currently do NOT use the java.net.HttpURLConnection and associated http client
 and  these do not return valid HTTP error codes so if a failure occurs it
 is best to connect the webserver using a browser to look for additional error
 info.

 The secure access tests require a user named 'jduke' with a password of 'theduke'
 with a role of 'AuthorizedUser' in the servlet container.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.17.2.7 $
 */
public class WebIntegrationUnitTestCase extends JBossTestCase
{
   private String baseURL = "http://jduke:theduke@localhost:" + Integer.getInteger("web.port", 8080) + "/"; 
   private String baseURLNoAuth = "http://localhost:" + Integer.getInteger("web.port", 8080) + "/"; 
   
   public WebIntegrationUnitTestCase(String name)
   {
      super(name);
   }
   
   /** Access the http://localhost/jbosstest/APIServlet
    */
   public void testAPIServlet() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/APIServlet");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/EJBOnStartupServlet
    */
   public void testEJBOnStartupServlet() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/EJBOnStartupServlet");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/ENCServlet
    */
   public void testENCServlet() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/ENCServlet");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/EJBServlet
    */
   public void testEJBServlet() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/EJBServlet");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/EntityServlet
    */
   public void testEntityServlet() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/EntityServlet");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/StatefulSessionServlet
    */
   public void testStatefulSessionServlet() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/StatefulSessionServlet");
      accessURL(url);
      // Need a mechanism to force passivation...
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/UserTransactionServlet
    */
   public void testUserTransactionServlet() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/UserTransactionServlet");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/SpeedServlet
    */
   public void testSpeedServlet() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/SpeedServlet");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/snoop.jsp
    */
   public void testSnoopJSP() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/snoop.jsp");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/snoop.jsp
    */
   public void testSnoopJSPByPattern() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/test-snoop.snp");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/test-jsp-mapping
    */
   public void testSnoopJSPByMapping() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/test-jsp-mapping");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/classpath.jsp
    */
   public void testJSPClasspath() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/classpath.jsp");
      accessURL(url);
   }

   /** Access the http://localhost/jbosstest/ClientLoginServlet
    */
   public void testClientLoginServlet() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/ClientLoginServlet");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/restricted/SecureServlet
    */
   public void testSecureServlet() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/restricted/SecureServlet");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/restricted/SecureServlet
    */
   public void testSecureServletAndUnsecureAccess() throws Exception
   {
      getLog().info("+++ testSecureServletAndUnsecureAccess");
      URL url = new URL(baseURL+"jbosstest/restricted/SecureServlet");
      getLog().info("Accessing SecureServlet with valid login");
      accessURL(url);
      String baseURL2 = "http://localhost:" + Integer.getInteger("web.port", 8080) + '/';
      URL url2 = new URL(baseURL2+"jbosstest/restricted/UnsecureEJBServlet");
      getLog().info("Accessing SecureServlet with no login");
      accessURL(url2, HttpURLConnection.HTTP_UNAUTHORIZED);
   }
   /** Access the http://localhost/jbosstest/restricted/SecureServlet
    */
   public void testSecureServletWithBadPass() throws Exception
   {
      String baseURL = "http://jduke:badpass@localhost:" + Integer.getInteger("web.port", 8080) + '/';
      URL url = new URL(baseURL+"jbosstest/restricted/SecureServlet");
      accessURL(url, HttpURLConnection.HTTP_UNAUTHORIZED);
   }
   /** Access the http://localhost/jbosstest/restricted/SecureServlet
    */
   public void testSecureServletWithNoLogin() throws Exception
   {
      String baseURL = "http://localhost:" + Integer.getInteger("web.port", 8080) + '/';
      URL url = new URL(baseURL+"jbosstest/restricted/SecureServlet");
      accessURL(url, HttpURLConnection.HTTP_UNAUTHORIZED);
   }
   /** Access the http://localhost/jbosstest-not/unrestricted/SecureServlet
    */
   public void testNotJbosstest() throws Exception
   {
      String baseURL = "http://localhost:" + Integer.getInteger("web.port", 8080) + '/';
      URL url = new URL(baseURL+"jbosstest-not/unrestricted/SecureServlet");
      accessURL(url, HttpURLConnection.HTTP_OK);
   }
   /** Access the http://localhost/jbosstest/restricted/SecureEJBAccess
    */
   public void testSecureEJBAccess() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/restricted/SecureEJBAccess");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/restricted/include_ejb.jsp
    */
   public void testIncludeEJB() throws Exception
   {
      URL url = new URL(baseURL+"jbosstest/restricted/include_ejb.jsp");
      accessURL(url);
   }
   /** Access the http://localhost/jbosstest/UnsecureEJBAccess with method=echo
    * to test that an unsecured servlet cannot access a secured EJB method
    * that requires a valid permission. This should fail.
    */
   public void testUnsecureEJBAccess() throws Exception
   {
      URL url = new URL(baseURLNoAuth+"jbosstest/UnsecureEJBAccess?method=echo");
      accessURL(url, HttpURLConnection.HTTP_INTERNAL_ERROR);
   }
   /** Access the http://localhost/jbosstest/UnsecureEJBAccess with method=unchecked
    * to test that an unsecured servlet can access a secured EJB method that
    * only requires an authenticated user. This requires unauthenticated
    * identity support by the web security domain.
    */
   public void testUnsecureAnonEJBAccess() throws Exception
   {
      URL url = new URL(baseURLNoAuth+"jbosstest/UnsecureEJBAccess?method=unchecked");
      accessURL(url, HttpURLConnection.HTTP_OK);
   }

   public void testUnsecureRunAsServlet() throws Exception
   {
      URL url = new URL(baseURLNoAuth+"jbosstest/UnsecureRunAsServlet?method=checkRunAs");
      accessURL(url, HttpURLConnection.HTTP_OK);      
   }

   /** Deploy a second ear that include a notjbosstest-web.war to test ears
    with the same war names conflicting.
    Access the http://localhost/jbosstest-not2/unrestricted/SecureServlet
    */
   public void testNotJbosstest2() throws Exception
   {
      try 
      {
         deploy("jbosstest-web2.ear");
         String baseURL = "http://localhost:" + Integer.getInteger("web.port", 8080) + '/';
         URL url = new URL(baseURL+"jbosstest-not2/unrestricted/SecureServlet");
         accessURL(url, HttpURLConnection.HTTP_OK);
      }
      finally
      {
         undeploy("jbosstest-web2.ear");
      } // end of try-finally
   }

   /** Deploy a bad war and then redploy with a fixed war to test failed war
    * cleanup.
    * Access the http://localhost/redeploy/index.html
    * @todo check with authors that undeploying first package tests desired behaviour.
    */
   public void testBadWarRedeploy() throws Exception
   {
      try
      {
         deploy("bad-web.war");
         fail("The bad-web.war deployment did not fail");
      }
      catch(Exception e)
      {
         getLog().debug("bad-web.war failed as expected", e);
      }
      finally
      {
         undeploy("bad-web.war");
      } // end of try-finally
      try 
      {
         deploy("good-web.war");
         String baseURL = "http://localhost:" + Integer.getInteger("web.port", 8080) + '/';
         URL url = new URL(baseURL+"redeploy/index.html");
         accessURL(url, HttpURLConnection.HTTP_OK);
      }
      finally
      {
         undeploy("good-web.war");
      } // end of try-finally
   }

   /** Test of a war that accesses classes referred to via the war manifest
    * classpath. Access the http://localhost/manifest/classpath.jsp
    */
   public void testWarManifest() throws Exception
   {
      deploy("manifest-web.ear");
      try
      {
         String baseURL = "http://localhost:" + Integer.getInteger("web.port", 8080) + '/';
         URL url = new URL(baseURL+"manifest/classpath.jsp");
         accessURL(url, HttpURLConnection.HTTP_OK);
      }
      finally
      {
         undeploy("jbosstest-good.ear");
      }
   }

   public void testBadEarRedeploy() throws Exception
   {
      try
      {
         deploy("jbosstest-bad.ear");
         fail("The jbosstest-bad.ear deployment did not fail");
      }
      catch(Exception e)
      {
         getLog().debug("jbosstest-bad.ear failed as expected", e);
      }
      finally
      {
         undeploy("jbosstest-bad.ear");
      } // end of finally
      try 
      {
         deploy("jbosstest-good.ear");
         String baseURL = "http://localhost:" + Integer.getInteger("web.port", 8080) + '/';
         URL url = new URL(baseURL+"redeploy/index.html");
         accessURL(url, HttpURLConnection.HTTP_OK);
      }
      finally
      {
         undeploy("jbosstest-good.ear");
      } // end of try-finally
      
   }

   private void accessURL(URL url) throws Exception
   {
      accessURL(url, HttpURLConnection.HTTP_OK);
   }
   private void accessURL(URL url, int expectedHttpCode) throws Exception
   {
      try
      {
         getLog().debug("Connecting to: "+url);
         
         HttpClient httpConn = new HttpClient();
         GetMethod request = new GetMethod(url.toString());
         String userInfo = url.getUserInfo();
         if( userInfo != null )
         {
            UsernamePasswordCredentials auth = new UsernamePasswordCredentials(userInfo);
            httpConn.getState().setCredentials("JBossTest Servlets", url.getHost(), auth);
         }
         getLog().debug("RequestURI: "+request.getURI());
         int responseCode = httpConn.executeMethod(request);
         String response = request.getStatusText();
         getLog().debug("responseCode="+responseCode+", response="+response);
         String content = request.getResponseBodyAsString();
         getLog().debug(content);
         // Validate that we are seeing a 401 error
         assertTrue("Expected reply code:"+expectedHttpCode+", actual="+responseCode,
            responseCode == expectedHttpCode);
      }
      catch(IOException e)
      {
         throw e;
      }
   }

   /**
    * Setup the test suite.
    */
   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();
      suite.addTest(new TestSuite(WebIntegrationUnitTestCase.class));

      // Create an initializer for the test suite
      Test wrapper = new JBossTestSetup(suite)
      {
         protected void setUp() throws Exception
         {
            super.setUp();
            deploy("jbosstest-web.ear");
            flushAuthCache();
         }
         protected void tearDown() throws Exception
         {
            undeploy("jbosstest-web.ear");
            super.tearDown();

            // Remove all the messages created during this test
            getServer().invoke
            (
               new ObjectName("jboss.mq.destination:service=Queue,name=testQueue"),
               "removeAllMessages",
               new Object[0],
               new String[0]
            );
         
         }
      };
      return wrapper;
   }

}
