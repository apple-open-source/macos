package org.jboss.test;

import java.io.InputStreamReader;
import java.net.URL;
import javax.security.auth.login.AppConfigurationEntry;
import javax.security.auth.login.AppConfigurationEntry.LoginModuleControlFlag;

import junit.framework.TestCase;

import org.jboss.security.auth.login.SunConfigParser;
import org.jboss.security.auth.login.XMLLoginConfigImpl;

/** Tests of the Sun login configuration file format parser
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.2 $
 */
public class SunConfigParserTestCase extends TestCase
{

   public SunConfigParserTestCase(String name)
   {
      super(name);
   }

   /** Test the Sun config file parser directly.
    *
    * @throws Exception
    */
   public void testParser() throws Exception
   {
      XMLLoginConfigImpl config = new XMLLoginConfigImpl();
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      URL configURL = loader.getResource("login-config.conf");
      InputStreamReader configFile = new InputStreamReader(configURL.openStream());
      SunConfigParser.doParse(configFile, config, true);

      AppConfigurationEntry[] entry = config.getAppConfigurationEntry("case1");
      assertTrue("case1 entry != null", entry != null);
      assertTrue("case1.length == 2", entry.length == 2);
      assertTrue("case1[0].module == org.jboss.test.TestLoginModule",
         entry[0].getLoginModuleName().equals("org.jboss.test.TestLoginModule"));
      assertTrue("case1[0].flag == required",
         entry[0].getControlFlag() == LoginModuleControlFlag.REQUIRED);
      assertTrue("case1[0].option(name) == 1.1",
         entry[0].getOptions().get("name").equals("1.1"));
      assertTrue("case1[0].option(succeed) == true",
         entry[0].getOptions().get("succeed").equals("true"));
      assertTrue("case1[0].option(throwEx) == false",
         entry[0].getOptions().get("throwEx").equals("false"));

      entry = config.getAppConfigurationEntry("case2");
      assertTrue("case2 entry != null", entry != null);
      assertTrue("case2.length == 2", entry.length == 2);
      assertTrue("case2[0].module = org.jboss.test.TestLoginModule",
         entry[0].getLoginModuleName().equals("org.jboss.test.TestLoginModule")); 
      assertTrue("case2[0].flag == optional",
         entry[0].getControlFlag() == LoginModuleControlFlag.OPTIONAL);
      assertTrue("case2[1].option(name) == 2.2",
         entry[1].getOptions().get("name").equals("2.2"));
      assertTrue("case2[1].option(succeed) == false",
         entry[1].getOptions().get("succeed").equals("false"));
      assertTrue("case2[1].option(throwEx) == true",
         entry[1].getOptions().get("throwEx").equals("true"));
   }

   /** Test the Sun config file parser by creating a XMLLoginConfig with a
    * URL pointing to a Sun format config file.
    *
    * @throws Exception
    */
   public void testSunLoginConfig() throws Exception
   {
      XMLLoginConfigImpl config = new XMLLoginConfigImpl();
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      URL configURL = loader.getResource("login-config.conf");
      config.setConfigURL(configURL);
      config.loadConfig();

      AppConfigurationEntry[] entry = config.getAppConfigurationEntry("case1");
      assertTrue("case1 entry != null", entry != null);
      assertTrue("case1.length == 2", entry.length == 2);
      assertTrue("case1[0].module == org.jboss.test.TestLoginModule",
         entry[0].getLoginModuleName().equals("org.jboss.test.TestLoginModule"));
      assertTrue("case1[0].flag == required",
         entry[0].getControlFlag() == LoginModuleControlFlag.REQUIRED);
      assertTrue("case1[0].option(name) == 1.1",
         entry[0].getOptions().get("name").equals("1.1"));
      assertTrue("case1[0].option(succeed) == true",
         entry[0].getOptions().get("succeed").equals("true"));
      assertTrue("case1[0].option(throwEx) == false",
         entry[0].getOptions().get("throwEx").equals("false"));

      entry = config.getAppConfigurationEntry("case2");
      assertTrue("case2 entry != null", entry != null);
      assertTrue("case2.length == 2", entry.length == 2);
      assertTrue("case2[0].module = org.jboss.test.TestLoginModule",
         entry[0].getLoginModuleName().equals("org.jboss.test.TestLoginModule"));
      assertTrue("case2[0].flag == optional",
         entry[0].getControlFlag() == LoginModuleControlFlag.OPTIONAL);
      assertTrue("case2[1].option(name) == 2.2",
         entry[1].getOptions().get("name").equals("2.2"));
      assertTrue("case2[1].option(succeed) == false",
         entry[1].getOptions().get("succeed").equals("false"));
      assertTrue("case2[1].option(throwEx) == true",
         entry[1].getOptions().get("throwEx").equals("true"));
   }
}
