/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.query;

import junit.framework.TestCase;

import test.compliance.query.support.Trivial;

import java.util.Iterator;
import java.util.Set;
import java.util.HashSet;

import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectInstance;
import javax.management.ObjectName;

/**
 * Object Name Query tests.<p>
 *
 * TODO: More tests, more systematic?
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class QueryMBeansObjectNameOnlyTestCase
  extends TestCase
{
   // Attributes ----------------------------------------------------------------

   /**
    * The number of objects registered in a server
    */
   int implSize;

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public QueryMBeansObjectNameOnlyTestCase(String s)
   {
      super(s);

      // Determine the number of objects in the implementation
      MBeanServer server = MBeanServerFactory.createMBeanServer();
      implSize = server.getMBeanCount().intValue();
      MBeanServerFactory.releaseMBeanServer(server);
   }

   // Tests ---------------------------------------------------------------------

   /**
    * Test single bean found.
    */
   public void testExactFound()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer("QueryMBeans");
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance1")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance2"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain2:type=instance1"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain3:type=instance1"));
         resultMBeans = server.queryMBeans(new ObjectName("Domain1:type=instance1"), null);
         resultNames = server.queryNames(new ObjectName("Domain1:type=instance1"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, 0);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * Test single bean not found.
    */
   public void testExactNotFound()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer("QueryMBeans");
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance1"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance2"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain2:type=instance1"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain3:type=instance1"));
         resultMBeans = server.queryMBeans(new ObjectName("Domain2:type=instance2"), null);
         resultNames = server.queryNames(new ObjectName("Domain2:type=instance2"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, 0);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * Test Get All.
    */
   public void testGetAllMBeans()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer("QueryMBeans");
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance2")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain2:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain3:type=instance1")));
         resultMBeans = server.queryMBeans(new ObjectName("*:*"), null);
         resultNames = server.queryNames(new ObjectName("*:*"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, implSize);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * Test Get All.
    */
   public void testGetAllMBeans2()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer("QueryMBeans");
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance2")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain2:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain3:type=instance1")));
         resultMBeans = server.queryMBeans(new ObjectName(""), null);
         resultNames = server.queryNames(new ObjectName(""), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, implSize);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * Test Get All.
    */
   public void testGetAllMBeans3()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer("QueryMBeans");
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance2")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain2:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain3:type=instance1")));
         resultMBeans = server.queryMBeans(null, null);
         resultNames = server.queryNames(null, null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, implSize);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * Test one domain.
    */
   public void testGetOneDomain()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer("QueryMBeans");
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance2")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain2:type=instance1"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain3:type=instance1"));
         resultMBeans = server.queryMBeans(new ObjectName("Domain1:*"), null);
         resultNames = server.queryNames(new ObjectName("Domain1:*"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, 0);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * Test default domain.
    */
   public void testGetDefaultDomain()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer("Domain1");
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance2")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain2:type=instance1"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain3:type=instance1"));
         resultMBeans = server.queryMBeans(new ObjectName(":*"), null);
         resultNames = server.queryNames(new ObjectName(":*"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, 0);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * Test one property.
    */
   public void testGetOneProperty()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer("QueryMBeans");
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance1")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain1:type=instance2"));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain2:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Domain3:type=instance1")));
         resultMBeans = server.queryMBeans(new ObjectName("*:type=instance1"), null);
         resultNames = server.queryNames(new ObjectName("*:type=instance1"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, 0);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * This one is from the spec.
    */
   public void testSpecAll()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer();
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Printer,type=laser")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=laser,date=1993")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Socrates:description=Printer,type=laser,date=1993")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=ink")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Disk,capacity=2")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Disk,capacity=1")));
         resultMBeans = server.queryMBeans(new ObjectName("*:*"), null);
         resultNames = server.queryNames(new ObjectName("*:*"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, implSize);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * This one is from the spec.
    */
   public void testSpecDefault()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer();
         server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Printer,type=laser"));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=laser,date=1993")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Socrates:description=Printer,type=laser,date=1993"));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=ink")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Disk,capacity=2"));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Disk,capacity=1")));
         resultMBeans = server.queryMBeans(new ObjectName(":*"), null);
         resultNames = server.queryNames(new ObjectName(":*"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, 0);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * This one is from the spec.
    */
   public void testSpecMyDomain()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer();
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Printer,type=laser")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=laser,date=1993"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Socrates:description=Printer,type=laser,date=1993"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=ink"));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Disk,capacity=2")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Disk,capacity=1"));
         resultMBeans = server.queryMBeans(new ObjectName("MyDomain:*"), null);
         resultNames = server.queryNames(new ObjectName("MyDomain:*"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, 0);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * This one is from the spec.
    */
   public void testSpecAnyAnyDomain()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer();
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Printer,type=laser")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=laser,date=1993"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Socrates:description=Printer,type=laser,date=1993"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=ink"));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Disk,capacity=2")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Disk,capacity=1"));
         resultMBeans = server.queryMBeans(new ObjectName("??Domain:*"), null);
         resultNames = server.queryNames(new ObjectName("??Domain:*"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, 0);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * This one is from the spec.
    */
   public void testAsteriskDomAsterisk()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer();
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Printer,type=laser")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=laser,date=1993")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Socrates:description=Printer,type=laser,date=1993"));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=ink")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Disk,capacity=2")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Disk,capacity=1")));
         resultMBeans = server.queryMBeans(new ObjectName("*Dom*:*"), null);
         resultNames = server.queryNames(new ObjectName("*Dom*:*"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, 0);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * This one is from the spec.
    */
   public void testSpecLaserPrinters()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer();
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Printer,type=laser")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=laser,date=1993")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Socrates:description=Printer,type=laser,date=1993")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=ink"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Disk,capacity=2"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Disk,capacity=1"));
         resultMBeans = server.queryMBeans(new ObjectName("*:description=Printer,type=laser,*"), null);
         resultNames = server.queryNames(new ObjectName("*:description=Printer,type=laser,*"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, 0);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * This one is from the spec.
    */
   public void testSpecPrinters()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer();
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Printer,type=laser")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=laser,date=1993")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("Socrates:description=Printer,type=laser,date=1993")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Printer,type=ink")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("MyDomain:description=Disk,capacity=2"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("DefaultDomain:description=Disk,capacity=2"));
         resultMBeans = server.queryMBeans(new ObjectName("*:description=Printer,*"), null);
         resultNames = server.queryNames(new ObjectName("*:description=Printer,*"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, 0);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * Test suffix asterisk on domain.
    */
   public void testSuffixMatchManyDomain()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer("QueryMBeans");
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DomainA123:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DomainA321:type=instance2")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DomainA2224:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DomainA3:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("DomainA:type=instance1")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain:type=instance1"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Dom:type=instance1"));
         resultMBeans = server.queryMBeans(new ObjectName("DomainA*:*"), null);
         resultNames = server.queryNames(new ObjectName("DomainA*:*"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, 0);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   /**
    * Test prefix asterisk on domain.
    */
   public void testPrefixMatchManyDomain()
   {
      MBeanServer server = null;
      HashSet instances = new HashSet();
      Set resultMBeans = null;
      Set resultNames = null;
      try
      {
         server = MBeanServerFactory.createMBeanServer("QueryMBeans");
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("123ADomain:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("321ADomain:type=instance2")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("2224ADomain:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("3ADomain:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("3ADomai123ADomain:type=instance1")));
         instances.add(server.registerMBean(new Trivial(), 
                       new ObjectName("ADomain:type=instance1")));
         server.registerMBean(new Trivial(), 
                       new ObjectName("Domain:type=instance1"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("ADomai:type=instance1"));
         server.registerMBean(new Trivial(), 
                       new ObjectName("main:type=instance1"));
         resultMBeans = server.queryMBeans(new ObjectName("*ADomain:*"), null);
         resultNames = server.queryNames(new ObjectName("*ADomain:*"), null);
      }
      catch (Exception e)
      {
         fail(e.toString());
      }

      checkit(resultMBeans, resultNames, instances, 0);

      if (server != null)
         MBeanServerFactory.releaseMBeanServer(server);
   }

   // Support ----------------------------------------------------------------

   /**
    * Check the resultMBeans
    */
   private void checkit(Set resultMBeans, Set resultNames, HashSet expected,
                        int others)
   {
      // Quick tests
      assertEquals(expected.size() + others, resultMBeans.size());
      assertEquals(expected.size() + others, resultNames.size());

      // Get the expected ObjectNames
      HashSet expectedNames = new HashSet();
      Iterator iterator = expected.iterator();
      while (iterator.hasNext())
      {
         expectedNames.add(((ObjectInstance) iterator.next()).getObjectName());
      }

      // Check resultMBeans
      iterator = resultMBeans.iterator();
      while (iterator.hasNext())
      {
         ObjectInstance instance = (ObjectInstance) iterator.next();
         Iterator iterator2 = expected.iterator();
         boolean found = false;
         while (iterator2.hasNext())
         {
            if (iterator2.next().equals(instance))
            {
               iterator2.remove();
               found = true;
               break;
            }
         }
        if (found == false && 
            instance.getObjectName().getDomain().equals("JMImplementation") == false)
           fail("Unexpected instance " + instance.getObjectName());
      }

      // Check resultNames
      iterator = resultNames.iterator();
      while (iterator.hasNext())
      {
         ObjectName name = (ObjectName) iterator.next();
         Iterator iterator2 = expectedNames.iterator();
         boolean found = false;
         while (iterator2.hasNext())
         {
            if (iterator2.next().equals(name))
            {
               iterator2.remove();
               found = true;
               break;
            }
         }
        if (found == false &&
            name.getDomain().equals("JMImplementation") == false)
           fail("Unexpected name " + name);
      }
   }
}
