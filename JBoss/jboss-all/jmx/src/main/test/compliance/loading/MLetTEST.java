/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.loading;

import java.net.URL;

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

public class MLetTEST extends TestCase
{
   public MLetTEST(String s)
   {
      super(s);
   }

   public void testCreateAndRegister() throws Exception
   {
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      MLet mlet = new MLet();
      ObjectName name = new ObjectName("test:name=mlet");
   
      server.registerMBean(mlet, name);
   }
   
   public void testBasicMLetFileLoad() throws Exception
   {
      // NOTE: 
      // the urls used here are relative to the location of the build.xml
      
      final String MLET_URL = "file:./output/etc/test/compliance/loading/BasicConfig.mlet";
      
      // make sure the classes are loaded from mlet, not system cl
      try
      {
         DefaultLoaderRepository.loadClass("test.compliance.loading.support.Trivial");
         
         fail("class test.compliance.loading.support.Trivial was already found in CL repository.");
      }
      catch (ClassNotFoundException e)
      {
         // expected
      }
      // make sure the classes are loaded from mlet, not system cl
      try
      {
         DefaultLoaderRepository.loadClass("test.compliance.loading.support.Trivial2");
         
         fail("class test.compliance.loading.support.Trivial2 was already found in CL repository.");
      }
      catch (ClassNotFoundException e)
      {
         // expected
      }
      
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      MLet mlet = new MLet();
      ObjectName name = new ObjectName("test:name=mlet");
      server.registerMBean(mlet, name);
      
      server.invoke(name, "getMBeansFromURL", 
      new Object[] { MLET_URL },
      new String[] { String.class.getName() }
      );

      try 
      {         
         assertTrue(server.isRegistered(new ObjectName("test:name=Trivial")));
         assertTrue(server.isRegistered(new ObjectName("test:name=Trivial2")));
      }
      catch (AssertionFailedError e)
      {
         URL[] urls = mlet.getURLs();
         fail("FAILS IN RI: SUN JMX RI builds a malformed URL from an MLet text file URL '" +
              MLET_URL + "' resulting into MLET codebase URL '" + urls[0] + "' and therefore fails " +
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
   
   public void testMalformedURLLoad()
   {
      // NOTE:
      // the urls used here are relative to the location of the build.xml
      
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         MLet mlet = new MLet();
         ObjectName name = new ObjectName("test:name=mlet");
         server.registerMBean(mlet, name);
         
         server.invoke(name, "getMBeansFromURL",
         new Object[] { "output/etc/test/compliance/loading/BasicConfig.mlet" },
         new String[] { String.class.getName() }
         );
         
         // should not reach here
         fail("FAILS IN RI: Malformed URL in getMBeansURL() should result in ServiceNotFoundException thrown.");
      }
      catch (AssertionFailedError e)
      {
         // defensive: in case assertXXX() or fail() are later added
         throw e;
      }
      catch (MBeanException e)
      {
         assertTrue(e.getTargetException() instanceof ServiceNotFoundException);
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }   
   }
   
   public void testMissingMLetTagInLoad()
   {
      // NOTE:
      // the urls used here are relative to the location of the build.xml
      
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         MLet mlet = new MLet();
         ObjectName name = new ObjectName("test:name=mlet");
         server.registerMBean(mlet, name);
         
         server.invoke(name, "getMBeansFromURL",
         new Object[] { "file:./output/etc/test/compliance/loading/MissingMLET.mlet" },
         new String[] { String.class.getName() }
         );
         
         // should not reach here
         fail("MLet text file missing the MLET tag should result in ServiceNotFoundException thrown.");
      }
      catch (AssertionFailedError e)
      {
         // defensive: in case assertXXX() or fail() are added later
         throw e;
      }
      catch (MBeanException e)
      {
         assertTrue(e.getTargetException() instanceof ServiceNotFoundException);
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }

   public void testMissingMandatoryArchiveTagInLoad()
   {
      // NOTE:
      // the urls used here are relative to the location of the build.xml
      
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         MLet mlet = new MLet();
         ObjectName name = new ObjectName("test:name=mlet");
         server.registerMBean(mlet, name);
         
         server.invoke(name, "getMBeansFromURL",
         new Object[] { "file:./output/etc/test/compliance/loading/MissingMandatoryArchive.mlet" },
         new String[] { String.class.getName() }
         );
         
         // should not reach here
         fail("MLet text file missing mandatory ARCHIVE attribute should result in ServiceNotFoundException thrown.");
      }
      catch (AssertionFailedError e)
      {
         // defensive: in case assertXXX() or fail() are added later
         throw e;
      }
      catch (MBeanException e)
      {
         assertTrue(e.getTargetException() instanceof ServiceNotFoundException);
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }
   
   public void testMissingMandatoryCodeTagInLoad()
   {
      // NOTE:
      // the urls used here are relative to the location of the build.xml
      
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         MLet mlet = new MLet();
         ObjectName name = new ObjectName("test:name=mlet");
         server.registerMBean(mlet, name);
         
         server.invoke(name, "getMBeansFromURL",
         new Object[] { "file:./output/etc/test/compliance/loading/MissingMandatoryCode.mlet" },
         new String[] { String.class.getName() }
         );
         
         // should not reach here
         fail("MLet text file missing mandatory CODE attribute should result in ServiceNotFoundException thrown.");
      }
      catch (AssertionFailedError e)
      {
         // defensive: in case assertXXX() or fail() are added later
         throw e;
      }
      catch (MBeanException e)
      {
         assertTrue(e.getTargetException() instanceof ServiceNotFoundException);
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }         
   
   public void testArchiveListInMLet() throws Exception
   {
      // NOTE:
      // the urls used here are relative to the location of the build.xml
      
      final String MLET_URL = "file:./output/etc/test/compliance/loading/ArchiveList.mlet";
   
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      MLet mlet = new MLet();
      ObjectName name = new ObjectName("test:name=mlet");
      server.registerMBean(mlet, name);
      
      server.invoke(name, "getMBeansFromURL",
      new Object[] { MLET_URL },
      new String[] { String.class.getName() }
      );
      
      Class c = null;
      
      try
      {
         c  = mlet.loadClass("test.compliance.loading.support.AClass");
      }
      catch (ClassNotFoundException e)
      {
         URL[] urls = mlet.getURLs();
         fail("FAILS IN RI: SUN JMX RI builds a malformed URL from an MLet text file URL '" +
              MLET_URL + "' resulting into MLET codebase URL '" + urls[0] + "' and therefore fails " +
              "to load the required classes from the Java archive.");            
      }
      
      Object o = c.newInstance();
      
      server.setAttribute(new ObjectName("test:name=AnotherTrivial"), new Attribute("Something", o));
      o = server.getAttribute(new ObjectName("test:name=AnotherTrivial"), "Something");
      
      assertTrue(o.getClass().isAssignableFrom(c));
   }
   
   public void testUnexpectedEndOfFile()
   {
      // NOTE:
      // the urls used here are relative to the location of the build.xml
      
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         MLet mlet = new MLet();
         ObjectName name = new ObjectName("test:name=mlet");
         server.registerMBean(mlet, name);
         
         server.invoke(name, "getMBeansFromURL",
         new Object[] { "file:./output/etc/test/compliance/loading/UnexpectedEnd.mlet" },
         new String[] { String.class.getName() }
         );
         
         // should not reach here
         fail("Unexpected end of file from mlet text file did not cause an exception.");
      }
      catch (AssertionFailedError e)
      {
         throw e;
      }
      catch (MBeanException e)
      {
         assertTrue(e.getTargetException() instanceof ServiceNotFoundException);
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }
   
   public void testMissingEndMLetTag()
   {
      // NOTE:
      // the urls used here are relative to the location of the build.xml
      
      try
      {
         MBeanServer server = MBeanServerFactory.createMBeanServer();
         MLet mlet = new MLet();
         ObjectName name = new ObjectName("test:name=mlet");
         server.registerMBean(mlet, name);
         
         server.invoke(name, "getMBeansFromURL",
         new Object[] { "file:./output/etc/test/compliance/loading/MissingEndTag.mlet" },
         new String[] { String.class.getName() }
         );
      
         assertTrue(!server.isRegistered(new ObjectName("test:name=Trivial")));
      }
      catch (AssertionFailedError e)
      {
         throw e;
      }
      catch (MBeanException e)
      {
         assertTrue(e.getTargetException() instanceof ServiceNotFoundException);
      }
      catch (Throwable t)
      {
         fail("Unexpected error: " + t.toString());
      }
   }
   
}
