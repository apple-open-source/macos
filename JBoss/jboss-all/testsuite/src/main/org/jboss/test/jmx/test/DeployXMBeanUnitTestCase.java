/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jmx.test;

import javax.management.Attribute;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.naming.Name;

import org.jboss.security.SecurityAssociation;
import org.jboss.security.SimplePrincipal;
import org.jboss.test.JBossTestCase;
import org.jboss.test.jmx.xmbean.CustomType;
import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jnp.interfaces.Naming;

/** JBoss model mbean deployment tests
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.8 $
 */
public class DeployXMBeanUnitTestCase extends JBossTestCase
{
   private final static String PACKAGE_NAME = "user-xmbean.sar";

   public DeployXMBeanUnitTestCase(String name)
   {
      super(name);
   }

   /** Test the xdoclet generated xmbean descriptor
    * @throws Exception on failure
    */
   public void testDeployXdocletUserXMBean() throws Exception
   {
      getLog().info("+++ testDeployXdocletUserXMBean");
      ObjectName userName = new ObjectName("jboss.test:service=xmbean-user");
      RMIAdaptor server = getServer();
      try
      {
         deploy(PACKAGE_NAME);
         getLog().info("Testing "+userName);

         boolean isRegistered = server.isRegistered(userName);
         assertTrue(userName+" is registered", isRegistered);
         server.setAttribute(userName, new Attribute("ArtificialAttribute", "test-value"));
         String actual = (String)server.getAttribute(userName, "ArtificialAttribute");
         assertTrue("ArtificialAttribute should be 'test-value' rather than " + actual, "test-value".equals(actual));
      }
      finally
      {
         undeploy(PACKAGE_NAME);
      } // end of try-finally
   }

   /** Test the hand generated xmbean descriptor
    * @throws Exception on failure
    */
   public void testDeployUserXMBean() throws Exception
   {
      getLog().info("+++ testDeployUserXMBean");
      ObjectName user2Name = new ObjectName("jboss.test:service=xmbean-user2");
      RMIAdaptor server = getServer();
      try
      {
         deploy(PACKAGE_NAME);
         getLog().info("Testing "+user2Name);
         assertTrue(user2Name+" is registered", server.isRegistered(user2Name));

         // Validate that Attr1 is read-write
         String attr1 = (String) server.getAttribute(user2Name, "Attr1");
         getLog().info("Attr1: "+attr1);
         assertTrue("Attr1 == attr1-string", attr1.equals("attr1-string"));
         Attribute attr1Info = new Attribute("Attr1", "attr1-string#2");
         server.setAttribute(user2Name, attr1Info);
         attr1 = (String) server.getAttribute(user2Name, "Attr1");
         assertTrue("Attr1 == attr1-string#2", attr1.equals("attr1-string#2"));

         // Validate that Attr2 is read-write
         Integer attr2 = (Integer) server.getAttribute(user2Name, "Attr2");
         getLog().info("Attr2: "+attr2);
         assertTrue("Attr2 == 15", attr2.intValue() == 15);
         Attribute attr2Info = new Attribute("Attr2", new Integer(51));
         server.setAttribute(user2Name, attr2Info);
         attr2 = (Integer) server.getAttribute(user2Name, "Attr2");
         assertTrue("Attr2 == 51", attr2.intValue() == 51);

         // Validate that Attr3 is read-write
         CustomType attr3 = (CustomType) server.getAttribute(user2Name, "Attr3");
         getLog().info("Attr3: "+attr3);
         assertTrue("Attr3 == 15", attr3.toString().equals("{10.20}"));
         Attribute attr3Info = new Attribute("Attr3", new CustomType(11, 22));
         server.setAttribute(user2Name, attr3Info);
         attr3 = (CustomType) server.getAttribute(user2Name, "Attr3");
         assertTrue("Attr3 == 51", attr3.toString().equals("{11.22}"));

         // Validate that HashCode is read-only
         Integer hash = (Integer) server.getAttribute(user2Name, "HashCode");
         getLog().info("HashCode: "+hash);
         Attribute hashInfo = new Attribute("HashCode", new Integer(12345));
         try
         {
            server.setAttribute(user2Name, hashInfo);
            fail("Was able to set HashCode attribute");
         }
         catch(Exception e)
         {
            getLog().info("set HashCode attribute failed as expected", e);
         }

         // Validate that Pi is read-only
         Double pi = (Double) server.getAttribute(user2Name, "Pi");
         getLog().info("Pi: "+pi);
         assertTrue("Pi == 3.14159", pi.doubleValue() == 3.14159);
         Attribute piInfo = new Attribute("Pi", new Double(7.654321));
         try
         {
            server.setAttribute(user2Name, piInfo);
            fail("Was able to set Pi attribute");
         }
         catch(Exception e)
         {
            getLog().info("set Pi attribute failed as expected", e);
         }

         // Validate that SecMgr is read-write
         ObjectName defaultSecMgr = new ObjectName("jboss.security:service=JaasSecurityManager");
         ObjectName secMgr = (ObjectName) server.getAttribute(user2Name, "SecMgr");
         getLog().info("SecMgr: "+secMgr);
         assertTrue("SecMgr == jboss.security:service=JaasSecurityManager", secMgr.equals(defaultSecMgr));
         ObjectName newSecMgr = new ObjectName("jboss.security:service=JaasSecMgr2");
         Attribute secMgrInfo = new Attribute("SecMgr", newSecMgr);
         server.setAttribute(user2Name, secMgrInfo);
         secMgr = (ObjectName) server.getAttribute(user2Name, "SecMgr");
         assertTrue("SecMgr == jboss.security:service=JaasSecMgr2", secMgr.equals(newSecMgr));

         Object[] noopArgs = {};
         String[] noopSig = {};
         server.invoke(user2Name, "noop", noopArgs, noopSig);

         Object[] echoArgs = {"testDeployUserXMBean"};
         String[] echoSig = {"java.lang.String"};
         String rtn = (String) server.invoke(user2Name, "echoDate", echoArgs, echoSig);
         getLog().info("echoDate: "+rtn);
      }
      finally
      {
         undeploy(PACKAGE_NAME);
      } // end of try-finally
   }

   /** Test the hand generated xmbean descriptor that is embedded in the service
    * descriptor.
    * @throws Exception on failure
    */
   public void testDeployUserEmbeddedDescriptorXMBean() throws Exception
   {
      getLog().info("+++ testDeployUserEmbeddedDescriptorXMBean");
      ObjectName user2Name = new ObjectName("jboss.test:service=xmbean-user2");
      RMIAdaptor server = getServer();
      try
      {
         deploy("user2-xmbean-embedded.sar");
         getLog().info("Testing "+user2Name);
         assertTrue(user2Name+" is registered", server.isRegistered(user2Name));

         // Validate that Attr1 is read-write
         String attr1 = (String) server.getAttribute(user2Name, "Attr1");
         getLog().info("Attr1: "+attr1);
         assertTrue("Attr1 == attr1-string", attr1.equals("attr1-string"));
         Attribute attr1Info = new Attribute("Attr1", "attr1-string#2");
         server.setAttribute(user2Name, attr1Info);
         attr1 = (String) server.getAttribute(user2Name, "Attr1");
         assertTrue("Attr1 == attr1-string#2", attr1.equals("attr1-string#2"));

         // Validate that Attr2 is read-write
         Integer attr2 = (Integer) server.getAttribute(user2Name, "Attr2");
         getLog().info("Attr2: "+attr2);
         assertTrue("Attr2 == 15", attr2.intValue() == 15);
         Attribute attr2Info = new Attribute("Attr2", new Integer(51));
         server.setAttribute(user2Name, attr2Info);
         attr2 = (Integer) server.getAttribute(user2Name, "Attr2");
         assertTrue("Attr2 == 51", attr2.intValue() == 51);

         // Validate that Attr3 is read-write
         CustomType attr3 = (CustomType) server.getAttribute(user2Name, "Attr3");
         getLog().info("Attr3: "+attr3);
         assertTrue("Attr3 == 15", attr3.toString().equals("{10.20}"));
         Attribute attr3Info = new Attribute("Attr3", new CustomType(11, 22));
         server.setAttribute(user2Name, attr3Info);
         attr3 = (CustomType) server.getAttribute(user2Name, "Attr3");
         assertTrue("Attr3 == 51", attr3.toString().equals("{11.22}"));

         // Validate that HashCode is read-only
         Integer hash = (Integer) server.getAttribute(user2Name, "HashCode");
         getLog().info("HashCode: "+hash);
         Attribute hashInfo = new Attribute("HashCode", new Integer(12345));
         try
         {
            server.setAttribute(user2Name, hashInfo);
            fail("Was able to set HashCode attribute");
         }
         catch(Exception e)
         {
            getLog().info("set HashCode attribute failed as expected", e);
         }

         // Validate that Pi is read-only
         Double pi = (Double) server.getAttribute(user2Name, "Pi");
         getLog().info("Pi: "+pi);
         assertTrue("Pi == 3.14159", pi.doubleValue() == 3.14159);
         Attribute piInfo = new Attribute("Pi", new Double(7.654321));
         try
         {
            server.setAttribute(user2Name, piInfo);
            fail("Was able to set Pi attribute");
         }
         catch(Exception e)
         {
            getLog().info("set Pi attribute failed as expected", e);
         }

         // Validate that SecMgr is read-write
         ObjectName defaultSecMgr = new ObjectName("jboss.security:service=JaasSecurityManager");
         ObjectName secMgr = (ObjectName) server.getAttribute(user2Name, "SecMgr");
         getLog().info("SecMgr: "+secMgr);
         assertTrue("SecMgr == jboss.security:service=JaasSecurityManager", secMgr.equals(defaultSecMgr));
         ObjectName newSecMgr = new ObjectName("jboss.security:service=JaasSecMgr2");
         Attribute secMgrInfo = new Attribute("SecMgr", newSecMgr);
         server.setAttribute(user2Name, secMgrInfo);
         secMgr = (ObjectName) server.getAttribute(user2Name, "SecMgr");
         assertTrue("SecMgr == jboss.security:service=JaasSecMgr2", secMgr.equals(newSecMgr));

         Object[] noopArgs = {};
         String[] noopSig = {};
         server.invoke(user2Name, "noop", noopArgs, noopSig);

         Object[] echoArgs = {"testDeployUserXMBean"};
         String[] echoSig = {"java.lang.String"};
         String rtn = (String) server.invoke(user2Name, "echoDate", echoArgs, echoSig);
         getLog().info("echoDate: "+rtn);
      }
      finally
      {
         undeploy("user2-xmbean-embedded.sar");
      } // end of try-finally
   }

   /** Tests of the attribute value settings from the xmbean descriptor
    * @throws Exception on failure
    */
   public void testUserXMBeanValues() throws Exception
   {
      getLog().info("+++ testUserXMBeanValues");
      try
      {
         ObjectName user2Name1 = new ObjectName("jboss.test:service=xmbean-user2,version=values1");
         RMIAdaptor server = getServer();
         deploy("user2-xmbean.sar");
         getLog().info("Testing "+user2Name1);
         assertTrue(user2Name1+" is registered", server.isRegistered(user2Name1));
         String value = (String) server.getAttribute(user2Name1, "Attr1");
         assertTrue("value == Att1InitialValue, value="+value, value.equals("Att1InitialValue"));
      }
      finally
      {
         undeploy("user2-xmbean.sar");
      }
   }

   /** Tests of the attribute value settings from the xmbean descriptor
    * @throws Exception on failure
    */
   public void testUserXMBeanPersistentValues() throws Exception
   {
      getLog().info("+++ testUserXMBeanPersistentValues");
      RMIAdaptor server = getServer();
      deploy("RemoveUser2Store.bsh");

      // Deploy the RemoveUser2Store.bsh script
      ObjectName user2Name2 = new ObjectName("jboss.test:service=xmbean-user2,version=values2");
      try
      {
         deploy("user2-xmbean.sar");
         getLog().info("Testing "+user2Name2);
         assertTrue(user2Name2+" is registered", server.isRegistered(user2Name2));
         String value = (String) server.getAttribute(user2Name2, "Attr1");
         assertTrue("value == Att1InitialValue, value="+value, value.equals("Att1InitialValue"));
         // Update the Attr1 value
         Attribute attr1 = new Attribute("Attr1", "UpdatedAttr1Value");
         server.setAttribute(user2Name2, attr1);
         // Undeploy
         undeploy("user2-xmbean.sar");
         // Reploy and validate
         deploy("user2-xmbean.sar");
         value = (String) server.getAttribute(user2Name2, "Attr1");
         assertTrue("value == UpdatedAttr1Value, value="+value, value.equals("UpdatedAttr1Value"));
      }
      finally
      {
         undeploy("user2-xmbean.sar");
      }
      undeploy("RemoveUser2Store.bsh");
   }

   /** Test an xmbean deployment with a custom security interceptor
    * @throws Exception on failure
    */
   public void testSecuredXMBean() throws Exception
   {
      getLog().info("+++ testSecuredXMBean");
      ObjectName xmbean = new ObjectName("jboss.test:service=SecuredXMBean");
      RMIAdaptor server = getServer();
      try
      {
         deploy("interceptors-xmbean.sar");
         getLog().info("Testing "+xmbean);

         boolean isRegistered = server.isRegistered(xmbean);
         assertTrue(xmbean+" is registered", isRegistered);

         Object[] args = {"Hello"};
         String[] sig = {"java.lang.String"};
         try
         {
            server.invoke(xmbean, "secretEcho", args, sig);
            fail("Was able to invoke secretEcho");
         }
         catch(Exception e)
         {
            getLog().info("secretEcho op failed as expected", e);
         }

         String echo = (String) server.invoke(xmbean, "echo", args, sig);
         getLog().info("echo returned: "+echo);
      }
      finally
      {
         undeploy("interceptors-xmbean.sar");
      } // end of try-finally
   }

   /** Test an xmbean deployment with a custom security interceptor
    * @throws Exception on failure
    */
   public void testSecuredJndiXMBean() throws Exception
   {
      getLog().info("+++ testSecuredJndiXMBean");
      ObjectName xmbean = new ObjectName("jboss.test:service=Naming,secured=true,persistent=true");
      RMIAdaptor server = getServer();
      try
      {
         deploy("interceptors-xmbean.sar");
         getLog().info("Testing "+xmbean);

         boolean isRegistered = server.isRegistered(xmbean);
         assertTrue(xmbean+" is registered", isRegistered);

         // Lookup the Naming interface
         InitialContext ctx = this.getInitialContext();
         Naming naming = (Naming) ctx.lookup("secure/Naming");
         getLog().info("Found Naming proxy: "+naming);
         Name hello = ctx.getNameParser("").parse("Hello");

         // Try to create a binding without security context
         try
         {
            naming.bind(hello, "HelloBinding", "java.lang.String");
            fail("Was able to invoke secretEcho");
         }
         catch(Exception e)
         {
            getLog().info("bind op failed as expected", e);
         }

         SimplePrincipal jduke = new SimplePrincipal("jduke");
         SecurityAssociation.setPrincipal(jduke);
         SecurityAssociation.setCredential("theduke".toCharArray());
         naming.bind(hello, "HelloBinding", "java.lang.String");
         getLog().info("Was able to create Hello binding");

         SimplePrincipal guest = new SimplePrincipal("guest");
         SecurityAssociation.setPrincipal(guest);
         SecurityAssociation.setCredential("guest".toCharArray());
         try
         {
            naming.bind(hello, "HelloBinding2", "java.lang.String");
            fail("guest was able to create binding");
         }
         catch(Exception e)
         {
            getLog().info("guest bind op failed as expected", e);
         }
      }
      finally
      {
         undeploy("interceptors-xmbean.sar");
      } // end of try-finally
   }

   /** Test an xmbean deployment with a custom security interceptor and
    * persistence interceptor. This runs after the testSecuredJndiXMBean
    * test and access the 
    * @throws Exception on failure
    */
   public void testPersistentJndiXMBean() throws Exception
   {
      getLog().info("+++ testPersistentJndiXMBean");
      ObjectName xmbean = new ObjectName("jboss.test:service=Naming,secured=true,persistent=true");
      RMIAdaptor server = getServer();
      try
      {
         deploy("interceptors-xmbean.sar");
         getLog().info("Testing "+xmbean);

         boolean isRegistered = server.isRegistered(xmbean);
         assertTrue(xmbean+" is registered", isRegistered);

         // Lookup the Naming interface
         InitialContext ctx = this.getInitialContext();
         Naming naming = (Naming) ctx.lookup("secure/Naming");
         getLog().info("Found Naming proxy: "+naming);
         Name hello = ctx.getNameParser("").parse("Hello");

         SimplePrincipal jduke = new SimplePrincipal("jduke");
         SecurityAssociation.setPrincipal(jduke);
         SecurityAssociation.setCredential("theduke".toCharArray());

         // Lookup the previous Hello binding
         String value = (String) naming.lookup(hello);
         assertTrue("lookup(Hello) == HelloBinding", value.equals("HelloBinding"));
         getLog().info("lookup(Hello) = "+value);
      }
      finally
      {
         undeploy("interceptors-xmbean.sar");
      } // end of try-finally
   }

}// DeployXMBeanUnitTestCase
