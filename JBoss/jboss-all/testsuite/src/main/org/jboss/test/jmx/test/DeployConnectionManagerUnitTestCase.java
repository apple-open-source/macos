/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmx.test;

import java.net.InetAddress;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Set;

//test bean
import javax.ejb.*;
import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.MBeanRegistrationException;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.ReflectionException;
import javax.management.RuntimeMBeanException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;

import junit.framework.*;

import org.jboss.test.JBossTestCase;
import org.jboss.test.jmx.interfaces.*;

/**
 * @see       <related>
 * @author    <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version   $Revision: 1.6.2.4 $
 */
public class DeployConnectionManagerUnitTestCase
       extends JBossTestCase
{
   // Constants -----------------------------------------------------

   public static final String DS_JNDI_NAME = "java:/ConnectionManagerTestDS";
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------
   // Constructors --------------------------------------------------
   /**
    * Constructor for the DeployConnectionManagerUnitTestCase object
    *
    * @param name  Testcase name
    */
   public DeployConnectionManagerUnitTestCase(String name)
   {
      super(name);
   }

   // Public --------------------------------------------------------


   /**
    * Test that a connection factory can be deployed after the rar it depends on
    * is deployed. Use an ejb to make sure that the DataSource defined is
    * actually bound in jndi. Undeploy and make sure the mbean is no longer
    * registered.
    *
    * @exception Exception  Description of Exception
    */
   public void testAddRemoveConnectionManager() throws Exception
   {
      //deploy test session bean to look up datasource
      deploy("jmxtest.jar");

      InitialContext ctx = getInitialContext();
      TestDataSourceHome tdshome = (TestDataSourceHome)ctx.lookup("test/TestDataSource");
      TestDataSource tds = tdshome.create();

      //See if the testds is bound in jndi, perhaps from a previous run.
      assertTrue(DS_JNDI_NAME + " is already bound, perhaps from a previous run", !tds.isBound(DS_JNDI_NAME));
      //the mbean we are trying to deploy/undeploy
      ObjectName cmName = new ObjectName("jboss.jca:service=LocalTxCM,name=TestDS");

      //try to remove mbean if leftover...
      if (getServer().isRegistered(cmName))
      {
         getServer().unregisterMBean(cmName);
      }

      //check it isn't there already
      assertTrue("test mbean already registered", !getServer().isRegistered(cmName));
      //try to create the cm.
      assertTrue("server did not return an objectInstance",
            getServer().createMBean("org.jboss.resource.connectionmanager.LocalTxConnectionManager",
            cmName) != null);

      //check deployment registered cm
      assertTrue("test mbean not registered", getServer().isRegistered(cmName));

      ObjectName mcfName = new ObjectName("jboss.jca:service=LocalTxDS,name=TestDS");

      //try to remove mbean if leftover...
      if (getServer().isRegistered(mcfName))
      {
         getServer().unregisterMBean(mcfName);
      }

      //check it isn't there already
      assertTrue(mcfName+" is NOT registered", !getServer().isRegistered(mcfName));
      //try to create the RARDeployment.
      Object mbean = getServer().createMBean("org.jboss.resource.connectionmanager.RARDeployment",
         mcfName);
      assertTrue("server returned objectInstance", mbean != null);

      //check deployment registered RARDeployment
      assertTrue(mcfName+" is registered", getServer().isRegistered(mcfName));

      ObjectName mcpName = new ObjectName("jboss.jca:service=LocalTxPool,name=TestDS");

      //try to remove mbean if leftover...
      if (getServer().isRegistered(mcpName))
      {
         getServer().unregisterMBean(mcpName);
      }

      //check it isn't there already
      assertTrue(mcpName+" is NOT registered", !getServer().isRegistered(mcpName));
      //try to create the pool.
      Object pool = getServer().createMBean("org.jboss.resource.connectionmanager.JBossManagedConnectionPool",
         mcpName);
      assertTrue("server returned objectInstance", pool!= null);

      //check deployment registered connection factory loader
      assertTrue(mcpName+" is registered", getServer().isRegistered(mcpName));
      ObjectName serviceControllerName = new ObjectName("jboss.system:service=ServiceController");

      //anon block so I don't rename var al
      {
         AttributeList al = new AttributeList();
         al.add(new Attribute("JndiName", "ConnectionManagerTestDS"));
         al.add(new Attribute("ManagedConnectionPool", mcpName));
         al.add(new Attribute("CachedConnectionManager", new ObjectName("jboss.jca:service=CachedConnectionManager")));
         //al.add(new Attribute("SecurityDomainJndiName", "TestRealm"));
         al.add(new Attribute("JaasSecurityManagerService", new ObjectName("jboss.security:service=JaasSecurityManager")));
         al.add(new Attribute("TransactionManager", "java:/TransactionManager"));
         //try to set the attributes on the bean
         assertTrue("setAttributes returned null", getServer().setAttributes(cmName, al) != null);

      }//anon block so I don't rename var

      //RARDeployment
      {
         AttributeList al = new AttributeList();
         al.add(new Attribute("ManagedConnectionFactoryClass", "org.jboss.resource.adapter.jdbc.local.LocalManagedConnectionFactory"));
         //try to set the attributes on the bean
         assertTrue("setAttributes returned null", getServer().setAttributes(mcfName, al) != null);

      }//anon block so I don't rename var
      {
         AttributeList al = new AttributeList();
         al.add(new Attribute("ManagedConnectionFactoryName", mcfName));
         al.add(new Attribute("MinSize", new Integer(0)));
         al.add(new Attribute("MaxSize", new Integer(50)));
         al.add(new Attribute("BlockingTimeoutMillis", new Long(5000)));
         al.add(new Attribute("IdleTimeoutMinutes", new Integer(15)));
         al.add(new Attribute("Criteria", "ByContainer"));
         //try to set the attributes on the bean
         assertTrue("setAttributes returned null", getServer().setAttributes(mcpName, al) != null);

      }//anon block so I don't rename var

      //start the mbeans


      // create and configure the ManagedConnectionFactory
      invoke(serviceControllerName,
             "create",
             new Object[] {mcfName},
             new String[] {"javax.management.ObjectName"});
      invoke(serviceControllerName,
             "start",
             new Object[] {mcfName},
             new String[] {"javax.management.ObjectName"});

      assertTrue("State is not started", "Started".equals(getServer().getAttribute(mcfName, "StateString")));
      //Now set the important attributes:
      invoke(mcfName, "setManagedConnectionFactoryAttribute",
             new Object[] {"ConnectionURL", java.lang.String.class, "jdbc:hsqldb:."},
             new String[] {"java.lang.String", "java.lang.Class", "java.lang.Object"});
      invoke(mcfName, "setManagedConnectionFactoryAttribute",
             new Object[] {"DriverClass", java.lang.String.class, "org.hsqldb.jdbcDriver"},
             new String[] {"java.lang.String", "java.lang.Class", "java.lang.Object"});
      invoke(mcfName, "setManagedConnectionFactoryAttribute",
             new Object[] {"UserName", java.lang.String.class, "sa"},
             new String[] {"java.lang.String", "java.lang.Class", "java.lang.Object"});
      invoke(mcfName, "setManagedConnectionFactoryAttribute",
             new Object[] {"Password", java.lang.String.class, ""},
             new String[] {"java.lang.String", "java.lang.Class", "java.lang.Object"});
      //start the pool
      invoke(serviceControllerName,
             "create",
             new Object[] {mcpName},
             new String[] {"javax.management.ObjectName"});
      invoke(serviceControllerName,
             "start",
             new Object[] {mcpName},
             new String[] {"javax.management.ObjectName"});

      assertTrue("State is not started", "Started".equals(getServer().getAttribute(mcpName, "StateString")));
      // start the ConnectionManager
      invoke(serviceControllerName,
             "create",
             new Object[] {cmName},
             new String[] {"javax.management.ObjectName"});
      invoke(serviceControllerName,
             "start",
             new Object[] {cmName},
             new String[] {"javax.management.ObjectName"});


      //see if the ConnectionFactory was loaded and works
      tds.testDataSource(DS_JNDI_NAME);

      //undeploy test connection factory loader.
      invoke(serviceControllerName,
             "stop",
             new Object[] {cmName},
             new String[] {"javax.management.ObjectName"});
      //See if the testds is bound in jndi, perhaps from a previous run.
      assertTrue(DS_JNDI_NAME + " is still bound, after stopping the cm", !tds.isBound(DS_JNDI_NAME));
      invoke(serviceControllerName,
             "destroy",
             new Object[] {cmName},
             new String[] {"javax.management.ObjectName"});
      invoke(serviceControllerName,
             "remove",
             new Object[] {cmName},
             new String[] {"javax.management.ObjectName"});

      invoke(serviceControllerName,
             "stop",
             new Object[] {mcfName},
             new String[] {"javax.management.ObjectName"});
      invoke(serviceControllerName,
             "destroy",
             new Object[] {mcfName},
             new String[] {"javax.management.ObjectName"});
      invoke(serviceControllerName,
             "remove",
             new Object[] {mcfName},
             new String[] {"javax.management.ObjectName"});

      invoke(serviceControllerName,
             "stop",
             new Object[] {mcpName},
             new String[] {"javax.management.ObjectName"});
      invoke(serviceControllerName,
             "destroy",
             new Object[] {mcpName},
             new String[] {"javax.management.ObjectName"});
      invoke(serviceControllerName,
             "remove",
             new Object[] {mcpName},
             new String[] {"javax.management.ObjectName"});

      //undeploy the test ejb
      undeploy("jmxtest.jar");

      //check that the connection factory loader is no longer registered
      assertTrue("connection factory loader mbean still registered", !getServer().isRegistered(cmName));

   }

}
