/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.naming.test;

import java.net.InetAddress;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Set;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.RuntimeMBeanException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;

import org.jboss.jmx.connector.RemoteMBeanServer;

import org.jboss.test.JBossTestCase;

/**
 * A test of the ExternalContext naming mbean. To test there needs to be one or
 * more ExternalContex mbeans setup. An example filesystem context setup would
 * be: 
  <mbean code="org.jboss.naming.ExternalContext" name="jboss:service=ExternalContext,jndiName=external/fs/tmp">
    <attribute name="JndiName">external/fs/Scott</attribute>
    <attribute name="Properties">tmp.fs</attribute>
    <attribute name="RemoteAccess">true</attribute>
  </mbean>

where tmp.fs is a Properties file containing:
# JNDI properties for /Scott filesystem directory
java.naming.factory.initial=com.sun.jndi.fscontext.RefFSContextFactory
java.naming.provider.url=file:/tmp

 *
 * @author    Scott_Stark@displayscape.com
 * @version   $Revision: 1.5 $
 */
public class ExternalContextUnitTestCase extends JBossTestCase
{
   private ObjectName[] contextNames;

//    private RemoteMBeanServer server;

   /**
    * Constructor for the ExternalContextUnitTestCase object
    *
    * @param name Testcase name
    */
   public ExternalContextUnitTestCase(String name)
   {
      super(name);
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testExternalContexts() throws Exception
   {
      if (contextNames == null)
      {
         getLog().debug("No ExternalContext names exist");
         return;
      }

      for (int n = 0; n < contextNames.length; n++)
      {
         ObjectName name = contextNames[n];
         String jndiName = name.getKeyProperty("jndiName");
         if (jndiName == null)
         {
            getLog().debug("Skipping " + name + " as it has no jndiName property");
            continue;
         }
         Context ctx = (Context)getInitialContext().lookup(jndiName);
         getLog().debug("+++ Listing for: " + ctx);
         list(ctx);
      }
   }

   /**
    * The JUnit setup method
    *
    * @exception Exception  Description of Exception
    */
   protected void setUp() throws Exception
   {
      try
      {
         super.setUp();
         contextNames = null;
         ObjectName pattern = new ObjectName("*:service=ExternalContext,*");
         Set names = getServer().queryMBeans(pattern, null);
         Iterator iter = names.iterator();
         ArrayList tmp = new ArrayList();
         while (iter.hasNext())
         {
            ObjectInstance oi = (ObjectInstance)iter.next();
            ObjectName name = oi.getObjectName();
            getLog().debug(name);
            tmp.add(name);
         }
         if (tmp.size() > 0)
         {
            contextNames = new ObjectName[tmp.size()];
            tmp.toArray(contextNames);
         }
      }
      catch (Exception x)
      {
         if (x instanceof RuntimeMBeanException)
         {
            getLog().error("setUp RuntimeMBeanException:",((RuntimeMBeanException)x).getTargetException());
         }
         else
         {
            getLog().error("setUp Error:" , x);
         }
      }
   }

   private void list(Context ctx) throws NamingException
   {
      NamingEnumeration enum = ctx.list("");
      while (enum.hasMore())
      {
         getLog().debug(enum.next());
      }
      enum.close();
   }

}
