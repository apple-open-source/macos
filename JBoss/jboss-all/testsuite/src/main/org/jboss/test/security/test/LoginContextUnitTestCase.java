package org.jboss.test.security.test;

import java.util.HashMap;
import javax.security.auth.Subject;
import javax.security.auth.login.AppConfigurationEntry;
import javax.security.auth.login.Configuration;
import javax.security.auth.login.LoginContext;

/** A JUnit TestCase for the JAAS LoginContext usage.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.3.4.1 $
 */
public class LoginContextUnitTestCase
   extends junit.framework.TestCase
{
   Subject subject1;
   Subject subject2;

   static class MyConfig extends Configuration
   {
      AppConfigurationEntry[] entry;
      MyConfig()
      {
         entry = new AppConfigurationEntry[1];
         HashMap opt0 = new HashMap();
         opt0.put("principal", "starksm");
         entry[0] = new AppConfigurationEntry("org.jboss.security.auth.spi.IdentityLoginModule", AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, opt0);
         //entry[1] = new AppConfigurationEntry("org.jboss.security.plugins.samples.RolesLoginModule", AppConfigurationEntry.LoginModuleControlFlag.REQUIRED, new HashMap());
      }

      public AppConfigurationEntry[] getAppConfigurationEntry(String appName)
      {
         return entry;
      }
      public void refresh()
      {
      }
   }

   public LoginContextUnitTestCase(String name)
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
      Configuration.setConfiguration(new MyConfig());
   }

   public void testLogin1() throws Exception
   {
      subject1 = new Subject();
      LoginContext lc = new LoginContext("LoginContext", subject1);
      lc.login();
      Subject lcSubject = lc.getSubject();
      assertTrue("subject == lcSubject",  subject1 == lcSubject );
   }
   public void testLogin2() throws Exception
   {
      subject2 = new Subject();
      LoginContext lc = new LoginContext("LoginContext", subject2);
      lc.login();
      Subject lcSubject = lc.getSubject();
      assertTrue("subject == lcSubject",  subject2 == lcSubject );
   }
}
