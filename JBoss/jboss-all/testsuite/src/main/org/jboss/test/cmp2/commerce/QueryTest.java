package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import java.util.Set;
import javax.management.MBeanServer;
import javax.management.MBeanServerFactory;
import javax.management.ObjectName;

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
import org.jboss.util.UnreachableStatementException;

public class QueryTest extends net.sourceforge.junitejb.EJBTestCase
{
   private JDBCEJBQLCompiler compiler;
   private static final String javaVersion = System.getProperty("java.specification.version");

   public static Test suite() throws Exception
   {
      return JBossTestCase.getDeploySetup(QueryTest.class, "cmp2-commerce.jar");
   }

   public QueryTest(String name)
   {
      super(name);
   }

   public void setUpEJB() throws Exception
   {
      MBeanServer server = (MBeanServer) MBeanServerFactory.findMBeanServer(null).get(0);
      ObjectName name = new ObjectName("jboss.j2ee:jndiName=commerce/Order,service=EJB");
      MBeanRegistry registry = (MBeanRegistry) MBeanProxyExt.create(MBeanRegistry.class, ServerConstants.MBEAN_REGISTRY, server);
      MBeanEntry entry = registry.get(name);
      EntityContainer container = (EntityContainer) entry.getResourceInstance();
      Catalog catalog = (Catalog) container.getEjbModule().getModuleData("CATALOG");
      compiler = new JDBCEJBQLCompiler(catalog);
   }

   private String compileEJBQL(String ejbql)
   {
      return compileEJBQL(ejbql, java.util.Collection.class, new Class[]{});
   }

   private String compileEJBQL(String ejbql, Class returnType, Class[] paramClasses)
   {
      try {
         compiler.compileEJBQL(ejbql, returnType, paramClasses, new JDBCReadAheadMetaData("on-load", 100, "*"));
         return compiler.getSQL().trim();
      } catch (Throwable t) {
         fail(t.getMessage());
         throw new UnreachableStatementException();
      }
   }

   private String compileJBossQL(String ejbql, Class returnType, Class[] paramClasses)
   {
      try {
         compiler.compileJBossQL(ejbql, returnType, paramClasses, new JDBCReadAheadMetaData("on-load", 100, "*"));
         return compiler.getSQL();
      } catch (Throwable t) {
         fail(t.getMessage());
         throw new UnreachableStatementException();
      }
   }

   public void testJBossQL() throws Exception
   {
      assertEquals("SELECT t0_u.USER_ID FROM USER_DATA t0_u WHERE (ucase(t0_u.USER_NAME) = ?)",
                   compileJBossQL("SELECT OBJECT(u) FROM user u WHERE UCASE(u.userName) = ?1",
                                  Collection.class, new Class[]{String.class}));

      assertEquals("SELECT t0_u.USER_ID FROM USER_DATA t0_u WHERE (lcase(t0_u.USER_NAME) = ?)",
                   compileJBossQL("SELECT OBJECT(u) FROM user u WHERE LCASE(u.userName) = ?1",
                                  Collection.class, new Class[]{String.class}));

      String expected = "1.4".equals(javaVersion) ?
            "SELECT t0_o1.ORDER_NUMBER FROM ORDER_DATA t0_o1, ORDER_DATA t3_o2, CUSTOMEREJB t2_o2_customer, CUSTOMEREJB t1_o1_customer WHERE (( NOT (t1_o1_customer.id=t2_o2_customer.id)) AND (t0_o1.CC_TYPE=t3_o2.CC_TYPE AND t0_o1.CC_FIRST_NAME=t3_o2.CC_FIRST_NAME AND t0_o1.CC_MI=t3_o2.CC_MI AND t0_o1.CC_LAST_NAME=t3_o2.CC_LAST_NAME AND t0_o1.CC_BILLING_ZIP=t3_o2.CC_BILLING_ZIP AND t0_o1.CC_CARD_NUMBER=t3_o2.CC_CARD_NUMBER) AND t3_o2.customer=t2_o2_customer.id AND t0_o1.customer=t1_o1_customer.id)" :
            "SELECT t0_o1.ORDER_NUMBER FROM ORDER_DATA t0_o1, ORDER_DATA t3_o2, CUSTOMEREJB t1_o1_customer, CUSTOMEREJB t2_o2_customer WHERE (( NOT (t1_o1_customer.id=t2_o2_customer.id)) AND (t0_o1.CC_TYPE=t3_o2.CC_TYPE AND t0_o1.CC_FIRST_NAME=t3_o2.CC_FIRST_NAME AND t0_o1.CC_MI=t3_o2.CC_MI AND t0_o1.CC_LAST_NAME=t3_o2.CC_LAST_NAME AND t0_o1.CC_BILLING_ZIP=t3_o2.CC_BILLING_ZIP AND t0_o1.CC_CARD_NUMBER=t3_o2.CC_CARD_NUMBER) AND t0_o1.customer=t1_o1_customer.id AND t3_o2.customer=t2_o2_customer.id)";
      String compiled = compileJBossQL(
         "SELECT OBJECT(o1) FROM OrderX o1, OrderX o2 WHERE o1.customer <> o2.customer AND o1.creditCard = o2.creditCard",
         Collection.class, new Class[]{});
      assertTrue("Expected: " + expected + " but got: " + compiled, expected.equals(compiled));

      assertEquals("SELECT t0_o.ORDER_NUMBER " +
                   "FROM ORDER_DATA t0_o " +
                   "WHERE ((t0_o.CC_TYPE=? " +
                   "AND t0_o.CC_FIRST_NAME=? " +
                   "AND t0_o.CC_MI=? " +
                   "AND t0_o.CC_LAST_NAME=? " +
                   "AND t0_o.CC_BILLING_ZIP=? " +
                   "AND t0_o.CC_CARD_NUMBER=?))",
                   compileJBossQL("SELECT OBJECT(o) FROM OrderX o WHERE o.creditCard = ?1",
                                  Collection.class, new Class[]{Card.class}));

      assertEquals("SELECT t0_o.ORDER_NUMBER " +
                   "FROM ORDER_DATA t0_o " +
                   "WHERE (( NOT (t0_o.CC_TYPE=? " +
                   "AND t0_o.CC_FIRST_NAME=? " +
                   "AND t0_o.CC_MI=? " +
                   "AND t0_o.CC_LAST_NAME=? " +
                   "AND t0_o.CC_BILLING_ZIP=? " +
                   "AND t0_o.CC_CARD_NUMBER=?)))",
                   compileJBossQL("SELECT OBJECT(o) FROM OrderX o WHERE o.creditCard <> ?1",
                                  Collection.class, new Class[]{Card.class}));

      assertEquals("SELECT DISTINCT t0_u.USER_ID, t0_u.USER_NAME FROM USER_DATA t0_u ORDER BY t0_u.USER_NAME ASC",
                   compileJBossQL("SELECT DISTINCT OBJECT(u) FROM user u ORDER BY u.userName", Collection.class, new Class[]{}));
      assertEquals("SELECT DISTINCT t0_u.USER_NAME, t0_u.USER_NAME FROM USER_DATA t0_u ORDER BY t0_u.USER_NAME ASC",
                   compileJBossQL("SELECT DISTINCT u.userName FROM user u ORDER BY u.userName", Collection.class, new Class[]{}));

      assertEquals(
         "SELECT t0_o.ORDER_NUMBER FROM ORDER_DATA t0_o, ADDRESSEJB t1_o_shippingAddress WHERE (t1_o_shippingAddress.city = ? AND t0_o.SHIPPING_ADDRESS=t1_o_shippingAddress.id) OR (t1_o_shippingAddress.state = ? AND t0_o.SHIPPING_ADDRESS=t1_o_shippingAddress.id)",
         compileJBossQL(
            "SELECT OBJECT(o) FROM OrderX o WHERE o.shippingAddress.city=?1 OR o.shippingAddress.state=?2",
            Collection.class,
            new Class[]{String.class, String.class}
         )
      );

      assertEquals(
         "SELECT t0_o.ORDER_NUMBER, t1_o_shippingAddress.state FROM ORDER_DATA t0_o, ADDRESSEJB t1_o_shippingAddress WHERE t0_o.SHIPPING_ADDRESS=t1_o_shippingAddress.id ORDER BY t1_o_shippingAddress.state ASC",
         compileJBossQL(
            "SELECT OBJECT(o) FROM OrderX o ORDER BY o.shippingAddress.state",
            Collection.class,
            new Class[]{String.class, String.class}
         )
      );
   }

   public void testEJBQL() throws Exception
   {
      assertEquals("SELECT t0_o.ORDER_NUMBER FROM ORDER_DATA t0_o",
                   compileEJBQL("SELECT OBJECT(o) FROM OrderX o"));

      assertEquals(
         "SELECT t0_o.ORDER_NUMBER FROM ORDER_DATA t0_o, ADDRESSEJB t1_o_shippingAddress WHERE (t1_o_shippingAddress.city = ? AND t0_o.SHIPPING_ADDRESS=t1_o_shippingAddress.id) OR (t1_o_shippingAddress.state = ? AND t0_o.SHIPPING_ADDRESS=t1_o_shippingAddress.id)",
         compileEJBQL(
            "SELECT OBJECT(o) FROM OrderX o WHERE o.shippingAddress.city=?1 OR o.shippingAddress.state=?2",
            Collection.class,
            new Class[]{String.class, String.class}
         )
      );

      String expected = "1.4".equals(javaVersion) ?
            "SELECT t0_o.ORDER_NUMBER FROM ORDER_DATA t0_o, LINEITEMEJB t4_l, PRODUCTCATEGORYEJB t1_pc, PRODUCT_PRODUCT_CATEGORY t5_l_product_productCategories_R, PRODUCT t6_l_product WHERE (((t0_o.ORDER_NUMBER = ? AND t1_pc.name = ?))) AND t6_l_product.id=t5_l_product_productCategories_R.PRODUCT_ID AND t1_pc.id=t5_l_product_productCategories_R.PRODUCT_CATEGORY_ID AND t4_l.product=t6_l_product.id AND t0_o.ORDER_NUMBER=t4_l.ORDER_NUMBER" :
            "SELECT t0_o.ORDER_NUMBER FROM ORDER_DATA t0_o, LINEITEMEJB t4_l, PRODUCTCATEGORYEJB t1_pc, PRODUCT_PRODUCT_CATEGORY t5_l_product_productCategories_R, PRODUCT t6_l_product WHERE (((t0_o.ORDER_NUMBER = ? AND t1_pc.name = ?))) AND t0_o.ORDER_NUMBER=t4_l.ORDER_NUMBER AND t6_l_product.id=t5_l_product_productCategories_R.PRODUCT_ID AND t1_pc.id=t5_l_product_productCategories_R.PRODUCT_CATEGORY_ID AND t4_l.product=t6_l_product.id";
      String compiled = compileEJBQL("SELECT OBJECT(o) FROM OrderX o, " +
                                      "IN(o.lineItems) l, " +
                                      "IN(l.product.productCategories) pc " +
                                      "WHERE (o.ordernumber = ?1 and pc.name=?2)",
                                      Collection.class, new Class[]{Long.class, String.class});
      assertEquals(expected, compiled);

      expected = "SELECT DISTINCT t0_o.ORDER_NUMBER " +
         "FROM ORDER_DATA t0_o, LINEITEMEJB t3_l " +
         "WHERE (t0_o.ORDER_NUMBER = ?) OR (EXISTS (SELECT t2_o_lineItems.id FROM LINEITEMEJB t2_o_lineItems " +
         "WHERE t0_o.ORDER_NUMBER=t2_o_lineItems.ORDER_NUMBER AND t2_o_lineItems.id=t3_l.id))";
      compiled = compileEJBQL("SELECT OBJECT(o) FROM OrderX o, LineItem l WHERE o.ordernumber = ?1 OR l MEMBER o.lineItems",
         Set.class, new Class[]{Long.class});
      assertTrue("Expected: " + expected + " but got: " + compiled, expected.equals(compiled));

      assertEquals("SELECT DISTINCT t0_o.ORDER_NUMBER " +
                   "FROM ORDER_DATA t0_o, LINEITEMEJB t3_l " +
                   "WHERE (t0_o.ORDER_NUMBER = ?) OR ( NOT EXISTS (SELECT t2_o_lineItems.id FROM LINEITEMEJB t2_o_lineItems " +
                   "WHERE t0_o.ORDER_NUMBER=t2_o_lineItems.ORDER_NUMBER AND t2_o_lineItems.id=t3_l.id))",
                   compileEJBQL("SELECT OBJECT(o) FROM OrderX o, LineItem l WHERE o.ordernumber = ?1 OR l NOT MEMBER o.lineItems",
                                Set.class, new Class[]{Long.class}));

      assertEquals("SELECT DISTINCT t0_p.id " +
                   "FROM PRODUCT t0_p, PRODUCTCATEGORYEJB t4_pc " +
                   "WHERE (t0_p.id = ?) OR (EXISTS (SELECT t3_p_productCategories_RELATION_.PRODUCT_CATEGORY_ID " +
                   "FROM PRODUCT_PRODUCT_CATEGORY t3_p_productCategories_RELATION_ " +
                   "WHERE t0_p.id=t3_p_productCategories_RELATION_.PRODUCT_ID " +
                   "AND t4_pc.id=t3_p_productCategories_RELATION_.PRODUCT_CATEGORY_ID))",
                   compileEJBQL("SELECT OBJECT(p) FROM Product p, ProductCategory pc WHERE p.id = ?1 OR pc MEMBER p.productCategories",
                                Set.class, new Class[]{Long.class}));

      assertEquals("SELECT DISTINCT t0_p.id " +
                   "FROM PRODUCT t0_p, PRODUCTCATEGORYEJB t4_pc " +
                   "WHERE (t0_p.id = ?) OR ( NOT EXISTS (SELECT t3_p_productCategories_RELATION_.PRODUCT_CATEGORY_ID " +
                   "FROM PRODUCT_PRODUCT_CATEGORY t3_p_productCategories_RELATION_ " +
                   "WHERE t0_p.id=t3_p_productCategories_RELATION_.PRODUCT_ID " +
                   "AND t4_pc.id=t3_p_productCategories_RELATION_.PRODUCT_CATEGORY_ID))",
                   compileEJBQL("SELECT OBJECT(p) FROM Product p, ProductCategory pc WHERE p.id = ?1 OR pc NOT MEMBER p.productCategories",
                                Set.class, new Class[]{Long.class}));

      assertEquals("SELECT DISTINCT t0_o.ORDER_NUMBER " +
                   "FROM ORDER_DATA t0_o " +
                   "WHERE (t0_o.ORDER_NUMBER = ?) OR (EXISTS (SELECT t2_o_lineItems.id " +
                   "FROM LINEITEMEJB t2_o_lineItems " +
                   "WHERE t0_o.ORDER_NUMBER=t2_o_lineItems.ORDER_NUMBER))",
                   compileEJBQL("SELECT OBJECT(o) FROM OrderX o WHERE o.ordernumber = ?1 OR o.lineItems IS NOT EMPTY",
                                Set.class, new Class[]{Long.class}));

      assertEquals("SELECT t0_l.id FROM CUSTOMEREJB t1_c, ORDER_DATA t3_o, LINEITEMEJB t0_l WHERE ((t1_c.id = 1)) AND t1_c.id=t3_o.customer AND t3_o.ORDER_NUMBER=t0_l.ORDER_NUMBER",
                   compileEJBQL("SELECT OBJECT(l) FROM Customer c, IN(c.orders) o, IN(o.lineItems) l WHERE c.id=1"));

      // customer query was SELECT OBJECT(s) FROM Service AS s, Platform AS p WHERE p.id = ?1 AND s.server MEMBER OF p.servers
      assertEquals("SELECT t0_l.id FROM LINEITEMEJB t0_l, CUSTOMEREJB t1_c, ORDER_DATA t3_l_order WHERE (t1_c.id = 1 AND EXISTS (SELECT t2_c_orders.ORDER_NUMBER FROM ORDER_DATA t2_c_orders WHERE t1_c.id=t2_c_orders.customer AND t2_c_orders.ORDER_NUMBER=t3_l_order.ORDER_NUMBER) AND t0_l.ORDER_NUMBER=t3_l_order.ORDER_NUMBER)",
                   compileEJBQL("SELECT OBJECT(l) FROM LineItem l, Customer c WHERE c.id=1 AND l.order MEMBER OF c.orders"));

      StringBuffer sql = new StringBuffer(200);
      sql.append("SELECT DISTINCT t0_li.id ")
         .append("FROM LINEITEMEJB t0_li, ORDER_DATA t1_li_order, ADDRESSEJB t2_li_order_billingAddress ")
         .append("WHERE (t1_li_order.BILLING_ADDRESS IS  NOT NULL AND t0_li.ORDER_NUMBER=t1_li_order.ORDER_NUMBER AND t1_li_order.BILLING_ADDRESS=t2_li_order_billingAddress.id)");
      assertEquals(
         sql.toString(),
         compileEJBQL("SELECT DISTINCT OBJECT(li) FROM LineItem AS li WHERE li.order.billingAddress IS NOT NULL")
      );
   }
}
