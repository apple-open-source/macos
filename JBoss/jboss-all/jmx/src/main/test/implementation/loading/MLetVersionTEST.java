/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.loading;

import java.net.URL;
import java.util.Date;
import java.util.List;

import junit.framework.TestCase;
import junit.framework.AssertionFailedError;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;
import javax.management.Attribute;
import javax.management.ServiceNotFoundException;
import javax.management.MBeanException;

import javax.management.loading.DefaultLoaderRepository;
import javax.management.loading.MLet;
import javax.management.loading.MLetMBean;

import org.jboss.mx.server.ServerConstants;
import org.jboss.mx.util.MBeanInstaller;

public class MLetVersionTEST extends TestCase
{
   public MLetVersionTEST(String s)
   {
      super(s);
   }

   public void testMLetFileLoadWithVersion() throws Exception
   {
      // NOTE: 
      // the urls used here are relative to the location of the build.xml
      
      final String MLET_URL1 = "file:./output/etc/test/implementation/loading/MLetVersion1.mlet";
      final String MLET_URL2 = "file:./output/etc/test/implementation/loading/MLetVersion2.mlet";

      //System.setProperty(ServerConstants.MBEAN_REGISTRY_CLASS_PROPERTY,
      //          "org.jboss.mx.server.registry.ManagedMBeanRegistry");

      ObjectName registry = new ObjectName(ServerConstants.MBEAN_REGISTRY);

      MBeanServer server = MBeanServerFactory.createMBeanServer();
      MLet mlet = new MLet();
      ObjectName name = new ObjectName("test:name=mlet");
      server.registerMBean(mlet, name);

      // Repeat to call the getMBeansFromURL method

      server.invoke(name, "getMBeansFromURL",
      new Object[] { MLET_URL1 },
      new String[] { String.class.getName() }
      );

      server.invoke(name, "getMBeansFromURL",
      new Object[] { MLET_URL2 },
      new String[] { String.class.getName() }
      );

      try
      {
         List versions1 =
               (List) server.invoke(registry, "getValue",
                                    new Object[]
                                    {
                                       new ObjectName("test:name=Trivial"),
                                       MBeanInstaller.VERSIONS
                                    },
                                    new String[]
                                    {
                                       ObjectName.class.getName(),
                                       String.class.getName()
                                    }
               );
         List versions2 =
               (List) server.invoke(registry, "getValue",
                                    new Object[]
                                    {
                                       new ObjectName("test:name=Trivial2"),
                                       MBeanInstaller.VERSIONS
                                    },
                                    new String[]
                                    {
                                       ObjectName.class.getName(),
                                       String.class.getName()
                                    }
               );

         assertTrue("Trivial1 version=" + versions1, ((String)versions1.get(0)).equals("1.1"));
         assertTrue("Trivial2 version=" + versions2, ((String)versions2.get(0)).equals("2.1"));
       }
      catch (MBeanException e)
      {
         e.printStackTrace();
         assertTrue(false);
      }

      try
      {
         assertTrue(server.isRegistered(new ObjectName("test:name=Trivial")));
         assertTrue(server.isRegistered(new ObjectName("test:name=Trivial2")));
      }
      catch (AssertionFailedError e)
      {
         URL[] urls = mlet.getURLs();
         fail("FAILS IN RI: SUN JMX RI builds a malformed URL from an MLet text file URL '" +
              MLET_URL1 + "' resulting into MLET codebase URL '" + urls[0] + "' and therefore fails " +
              "to load the required classes from the Java archive (MyMBeans.jar)");
      }

      assertTrue(server.getMBeanInfo(new ObjectName("test:name=Trivial")) != null);
      assertTrue(server.getMBeanInfo(new ObjectName("test:name=Trivial2")) != null);

      assertTrue(server.getAttribute(new ObjectName("test:name=Trivial2"), "Something").equals("foo"));

      server.invoke(new ObjectName("test:name=Trivial"), "doOperation",
      new Object[] { "Test" },
      new String[] { String.class.getName() }
      );

      server.invoke(new ObjectName("test:name=Trivial2"), "doOperation",
      new Object[] { "Test" },
      new String[] { String.class.getName() }
      );

   }

}
