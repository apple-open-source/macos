package org.jboss.test;

import java.net.URL;
import javax.security.auth.Policy;
import javax.security.auth.Subject;
import javax.security.auth.callback.CallbackHandler;
import javax.security.auth.login.Configuration;
import javax.security.auth.login.LoginContext;
import javax.security.auth.login.LoginException;

import org.jboss.security.SecurityPolicy;
import org.jboss.security.SecurityPolicyParser;
import org.jboss.security.auth.callback.UsernamePasswordHandler;

/** A test of the SRPLogin module
 
 @see org.jboss.security.srp.jaas.SRPLoginModule
 
@author Scott.Stark@jboss.org
@version $Revision: 1.3.4.1 $
*/
public class TestSRPLogin extends junit.framework.TestCase
{
   public TestSRPLogin(String name)
   {
      super(name);
   }
   
   /** Create a SecurityPolicy from a xml policy file and install it as the
    JAAS Policy and Configuration implementations.
    */
   protected void setUp() throws Exception
   {
      // Create a subject security policy
      String policyName = "tst-policy.xml";
      URL policyURL = getClass().getClassLoader().getResource(policyName);
      if( policyURL == null )
         throw new IllegalStateException("Failed to find "+policyName+" in classpath");
      SecurityPolicyParser policyStore = new SecurityPolicyParser(policyURL);
      SecurityPolicy policy = new SecurityPolicy(policyStore);
      policy.refresh();
      Policy.setPolicy(policy);
      Configuration.setConfiguration(policy.getLoginConfiguration());
   }
   
   public void testLogin()
   {
      CallbackHandler handler = new UsernamePasswordHandler("scott", "stark".toCharArray());
      try
      {
         LoginContext lc = new LoginContext("srp-login", handler);
         lc.login();
         Subject subject = lc.getSubject();
         System.out.println("Subject="+subject);
      }
      catch(LoginException e)
      {
         e.printStackTrace();
         fail(e.getMessage());
      }
   }
   
   public static void main(String args[])
   {
      try
      {
         TestSRPLogin tst = new TestSRPLogin("main");
         tst.setUp();
      }
      catch(Exception e)
      {
         e.printStackTrace(System.out);
      }
   }
}
