package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import java.util.Set;
import java.util.Iterator;
import java.util.HashSet;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;
import javax.naming.Context;
import javax.naming.InitialContext;

import junit.framework.Test;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.plugins.cmp.ejbql.Catalog;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCEJBQLCompiler;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCReadAheadMetaData;
import org.jboss.mx.server.ServerConstants;
import org.jboss.mx.server.registry.MBeanEntry;
import org.jboss.mx.server.registry.MBeanRegistry;
import org.jboss.test.JBossTestCase;
import org.jboss.mx.util.MBeanProxyExt;

public class LimitOffsetTest extends net.sourceforge.junitejb.EJBTestCase {
   private JDBCEJBQLCompiler compiler;
   private Class[] params = { int.class, int.class };
   private JDBCReadAheadMetaData readAheadMetaData;
   private OrderHome orderHome;

   public static Test suite() throws Exception {
		return JBossTestCase.getDeploySetup(
            LimitOffsetTest.class,
            "cmp2-commerce.jar");
   }


   public LimitOffsetTest(String name) {
      super(name);
   }

   public void setUpEJB() throws Exception
   {
      MBeanServer server = (MBeanServer) MBeanServerFactory.findMBeanServer(null).get(0);
      ObjectName name = new ObjectName("jboss.j2ee:jndiName=commerce/Order,service=EJB");
      MBeanRegistry registry = (MBeanRegistry) MBeanProxyExt.create(MBeanRegistry.class,
                                                   ServerConstants.MBEAN_REGISTRY,
                                                   server);
      MBeanEntry entry = registry.get(name);
      EntityContainer container = (EntityContainer) entry.getResourceInstance();
      Catalog catalog = (Catalog) container.getEjbModule().getModuleData("CATALOG");
      compiler = new JDBCEJBQLCompiler(catalog);
      readAheadMetaData = new JDBCReadAheadMetaData("on-load", 100, "*");

      Context ctx = new InitialContext();
      orderHome = (OrderHome) ctx.lookup("commerce/Order");

      for (Iterator i = orderHome.findAll().iterator(); i.hasNext(); )
      {
         Order order = (Order) i.next();
         i.remove();
         order.remove();
      }

      for (int i=100; i < 110; i++)
      {
         orderHome.create(new Long(i));
      }
   }

   public void testCompiler() throws Exception
   {
      compiler.compileJBossQL("SELECT OBJECT(o) FROM OrderX o", Collection.class, params, readAheadMetaData);
      assertEquals("SELECT t0_o.ORDER_NUMBER FROM ORDER_DATA t0_o", compiler.getSQL());
      assertEquals(0, compiler.getOffsetParam());
      assertEquals(0, compiler.getLimitParam());

      compiler.compileJBossQL("SELECT OBJECT(o) FROM OrderX o OFFSET ?2", Collection.class, params, readAheadMetaData);
      assertEquals("SELECT t0_o.ORDER_NUMBER FROM ORDER_DATA t0_o", compiler.getSQL());
      assertEquals(2, compiler.getOffsetParam());
      assertEquals(0, compiler.getLimitParam());

      compiler.compileJBossQL("SELECT OBJECT(o) FROM OrderX o LIMIT ?1", Collection.class, params, readAheadMetaData);
      assertEquals("SELECT t0_o.ORDER_NUMBER FROM ORDER_DATA t0_o", compiler.getSQL());
      assertEquals(0, compiler.getOffsetParam());
      assertEquals(1, compiler.getLimitParam());

      compiler.compileJBossQL("SELECT OBJECT(o) FROM OrderX o OFFSET ?1 LIMIT ?2", Collection.class, params, readAheadMetaData);
      assertEquals("SELECT t0_o.ORDER_NUMBER FROM ORDER_DATA t0_o", compiler.getSQL());
      assertEquals(1, compiler.getOffsetParam());
      assertEquals(2, compiler.getLimitParam());

      compiler.compileJBossQL("SELECT OBJECT(o) FROM OrderX o OFFSET 1 LIMIT 2", Collection.class, null, readAheadMetaData);
      assertEquals("SELECT t0_o.ORDER_NUMBER FROM ORDER_DATA t0_o", compiler.getSQL());
      assertEquals(0, compiler.getOffsetParam());
      assertEquals(0, compiler.getLimitParam());
      assertEquals(1, compiler.getOffsetValue());
      assertEquals(2, compiler.getLimitValue());

      try
      {
         compiler.compileJBossQL("SELECT OBJECT(o) FROM OrderX o OFFSET ?1", Collection.class, new Class[] { long.class }, readAheadMetaData);
         fail("Expected Exception due to non-int argument");
      }
      catch (Exception e)
      {
         // OK
      }
   }

   public void testLimitOffset() throws Exception
   {
      Set result;
      result = orderHome.getStuff("SELECT OBJECT(o) FROM OrderX o", new Object[] { } );
      checkKeys(result, new long[] { 100, 101, 102, 103, 104, 105, 106, 107, 108, 109});

      result = orderHome.getStuff("SELECT OBJECT(o) FROM OrderX o LIMIT ?1", new Object[] { new Integer(3) } );
      checkKeys(result, new long[] { 100, 101, 102 });

      result = orderHome.getStuff("SELECT OBJECT(o) FROM OrderX o OFFSET ?1", new Object[] { new Integer(3) } );
      checkKeys(result, new long[] { 103, 104, 105, 106, 107, 108, 109 });

      result = orderHome.getStuff("SELECT OBJECT(o) FROM OrderX o OFFSET ?1 LIMIT ?2", new Object[] { new Integer(0), new Integer(3) } );
      checkKeys(result, new long[] { 100, 101, 102 });

      result = orderHome.getStuff("SELECT OBJECT(o) FROM OrderX o OFFSET ?1 LIMIT ?2", new Object[] { new Integer(3), new Integer(3) } );
      checkKeys(result, new long[] { 103, 104, 105 });

      result = orderHome.getStuff("SELECT OBJECT(o) FROM OrderX o OFFSET ?1 LIMIT ?2", new Object[] { new Integer(6), new Integer(3) } );
      checkKeys(result, new long[] { 106, 107, 108 });

      result = orderHome.getStuff("SELECT OBJECT(o) FROM OrderX o OFFSET ?1 LIMIT ?2", new Object[] { new Integer(9), new Integer(3) } );
      checkKeys(result, new long[] { 109 });
   }

   public void testFinderWithLimitOffset() throws Exception
   {
      Collection result;
      result = orderHome.findWithLimitOffset(6, 3);
      checkKeys(result, new long[] { 106, 107, 108 });
   }

   private void checkKeys(Collection c, long[] expected)
   {
      assertEquals(expected.length, c.size());
      Set expectedSet = new HashSet(expected.length);
      for (int i = 0; i < expected.length; i++)
      {
         long l = expected[i];
         expectedSet.add(new Long(l));
      }

      Set actualSet = new HashSet(c.size());
      for (Iterator iterator = c.iterator(); iterator.hasNext();)
      {
         Order order = (Order) iterator.next();
         actualSet.add(order.getPrimaryKey());
      }

      assertEquals(expectedSet, actualSet);
   }
}
