package org.jboss.test.cmp2.relationship.oneToOneUnidirectional;

import java.util.Collection;
import java.util.Iterator;
import javax.naming.InitialContext;
import junit.framework.Test;
import junit.framework.TestCase;
import net.sourceforge.junitejb.EJBTestCase;
import org.jboss.test.JBossTestCase;

public class ABTest extends EJBTestCase {
    static org.apache.log4j.Category log =
       org.apache.log4j.Category.getInstance(ABTest.class);

	public static Test suite() throws Exception {
		return JBossTestCase.getDeploySetup(ABTest.class, "cmp2-relationship.jar");
   }

   public ABTest(String name) {
      super(name);
   }

   private AHome getTableAHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (AHome) 
               jndiContext.lookup("relation/oneToOne/unidirectional/table/A");
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getTableAHome: " + e.getMessage());
      }
      return null;
   }

   private BHome getTableBHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (BHome) 
               jndiContext.lookup("relation/oneToOne/unidirectional/table/B");
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getTableBHome: " + e.getMessage());
      }
      return null;
   }

   private AHome getFKAHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (AHome) 
               jndiContext.lookup("relation/oneToOne/unidirectional/fk/A");
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getFKAHome: " + e.getMessage());
      }
      return null;
   }

   private BHome getFKBHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (BHome) 
               jndiContext.lookup("relation/oneToOne/unidirectional/fk/B");
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getFKBHome: " + e.getMessage());
      }
      return null;
   }

   private A a1;
   private A a2;
   private B b1;
   private B b2;

   protected void beforeChange(AHome aHome, BHome bHome) throws Exception {
      a1 = aHome.create(new Integer(1));
      a2 = aHome.create(new Integer(2));
      b1 = bHome.create(new Integer(10));
      b2 = bHome.create(new Integer(20));
      a1.setB(b1);
      a2.setB(b2);

      assertTrue(b1.isIdentical(a1.getB()));
      assertTrue(b2.isIdentical(a2.getB()));
   }

   // a1.setB(a2.getB());
   public void test_a1SetB_a2GetB_Table() throws Exception {
      AHome aHome = getTableAHome();
      BHome bHome = getTableBHome();

      beforeChange(aHome, bHome);
      a1SetB_a2GetB(aHome, bHome);
   }

   // a1.setB(a2.getB());
   public void test_a1SetB_a2GetB_FK() throws Exception {
      AHome aHome = getFKAHome();
      BHome bHome = getFKBHome();
      beforeChange(aHome, bHome);
      a1SetB_a2GetB(aHome, bHome);
   }

   // a1.setB(a2.getB());
   protected void a1SetB_a2GetB(AHome aHome, BHome bHome) throws Exception {
      // Change:
      a1.setB(a2.getB());

      // Expected result:

      // b2.isIdentical(a1.getB())
      assertTrue(b2.isIdentical(a1.getB()));

      // a2.getB() == null
      assertNull(a2.getB());
   }

   public void setUpEJB() throws Exception {
      AHome aHome;
      BHome bHome;

      aHome = getTableAHome();
      bHome = getTableBHome();
      deleteAllAsAndBs(aHome, bHome);

      aHome = getFKAHome();
      bHome = getFKBHome();
      deleteAllAsAndBs(aHome, bHome);
   }

   public void tearDownEJB() throws Exception {
   }

   public void deleteAllAsAndBs(AHome aHome, BHome bHome) throws Exception {
      // delete all As
      Iterator currentAs = aHome.findAll().iterator();
      while(currentAs.hasNext()) {
         A a = (A)currentAs.next();
         a.remove();
      }

      // delete all Bs
      Iterator currentBs = bHome.findAll().iterator();
      while(currentBs.hasNext()) {
         B b = (B)currentBs.next();
         b.remove();
      }
   }
}



