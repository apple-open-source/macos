package org.jboss.test.cmp2.relationship.manyToManyBidirectional;

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

   private AHome getAHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (AHome) jndiContext.lookup("relation/manyToMany/bidirectional/A"); 
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getAHome: " + e.getMessage());
      }
      return null;
   }

   private BHome getBHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (BHome) jndiContext.lookup("relation/manyToMany/bidirectional/B");
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getBHome: " + e.getMessage());
      }
      return null;
   }

   // a1.setB(a3.getB());
   public void test_a1SetB_a3GetB() throws Exception {
      AHome aHome = getAHome();
      BHome bHome = getBHome();

      // Before change:
      A a1 = aHome.create(new Integer(1));
      A a2 = aHome.create(new Integer(2));
      A a3 = aHome.create(new Integer(3));
      A a4 = aHome.create(new Integer(4));
      A a5 = aHome.create(new Integer(5));

      B b1 = bHome.create(new Integer(-1));
      B b2 = bHome.create(new Integer(-2));
      B b3 = bHome.create(new Integer(-3));
      B b4 = bHome.create(new Integer(-4));
      B b5 = bHome.create(new Integer(-5));
      
      a1.getB().add(b1);
      a1.getB().add(b2);
      a2.getB().add(b1);
      a2.getB().add(b2);
      a2.getB().add(b3);
      a3.getB().add(b2);
      a3.getB().add(b3);
      a3.getB().add(b4);
      a4.getB().add(b3);
      a4.getB().add(b4);
      a4.getB().add(b5);
      a5.getB().add(b4);
      a5.getB().add(b5);
      
      assertTrue(a1.getB().contains(b1));
      assertTrue(a1.getB().contains(b2));
      assertTrue(a2.getB().contains(b1));
      assertTrue(a2.getB().contains(b2));
      assertTrue(a2.getB().contains(b3));
      assertTrue(a3.getB().contains(b2));
      assertTrue(a3.getB().contains(b3));
      assertTrue(a3.getB().contains(b4));
      assertTrue(a4.getB().contains(b3));
      assertTrue(a4.getB().contains(b4));
      assertTrue(a4.getB().contains(b5));
      assertTrue(a5.getB().contains(b4));
      assertTrue(a5.getB().contains(b5));

      assertTrue(b1.getA().contains(a1));
      assertTrue(b1.getA().contains(a2));
      assertTrue(b2.getA().contains(a1));
      assertTrue(b2.getA().contains(a2));
      assertTrue(b2.getA().contains(a3));
      assertTrue(b3.getA().contains(a2));
      assertTrue(b3.getA().contains(a3));
      assertTrue(b3.getA().contains(a4));
      assertTrue(b4.getA().contains(a3));
      assertTrue(b4.getA().contains(a4));
      assertTrue(b4.getA().contains(a5));
      assertTrue(b5.getA().contains(a4));
      assertTrue(b5.getA().contains(a5));

      // Change:
      a1.setB(a3.getB());
      
      // Expected result:
      assertTrue(!a1.getB().contains(b1));
      assertTrue(a1.getB().contains(b2));
      assertTrue(a1.getB().contains(b3));
      assertTrue(a1.getB().contains(b4));
      
      assertTrue(a2.getB().contains(b1));
      assertTrue(a2.getB().contains(b2));
      assertTrue(a2.getB().contains(b3));
      
      assertTrue(a3.getB().contains(b2));
      assertTrue(a3.getB().contains(b3));
      assertTrue(a3.getB().contains(b4));
      
      assertTrue(a4.getB().contains(b3));
      assertTrue(a4.getB().contains(b4));
      assertTrue(a4.getB().contains(b5));
      
      assertTrue(a5.getB().contains(b4));
      assertTrue(a5.getB().contains(b5));
      
      
      assertTrue(!b1.getA().contains(a1));
      assertTrue(b1.getA().contains(a2));
      
      assertTrue(b2.getA().contains(a1));
      assertTrue(b2.getA().contains(a2));
      assertTrue(b2.getA().contains(a3));
      
      assertTrue(b3.getA().contains(a1));
      assertTrue(b3.getA().contains(a2));
      assertTrue(b3.getA().contains(a3));
      assertTrue(b3.getA().contains(a4));
      
      assertTrue(b4.getA().contains(a1));
      assertTrue(b4.getA().contains(a3));
      assertTrue(b4.getA().contains(a4));
      assertTrue(b4.getA().contains(a5));

      assertTrue(b5.getA().contains(a4));
      assertTrue(b5.getA().contains(a5));
   }
   
   // a1.getB().add(b3);
   public void test_a1GetB_addB3() throws Exception {
      AHome aHome = getAHome();
      BHome bHome = getBHome();

      // Before change:
      A a1 = aHome.create(new Integer(1));
      A a2 = aHome.create(new Integer(2));
      A a3 = aHome.create(new Integer(3));
      A a4 = aHome.create(new Integer(4));
      A a5 = aHome.create(new Integer(5));

      B b1 = bHome.create(new Integer(-1));
      B b2 = bHome.create(new Integer(-2));
      B b3 = bHome.create(new Integer(-3));
      B b4 = bHome.create(new Integer(-4));
      B b5 = bHome.create(new Integer(-5));
      
      a1.getB().add(b1);
      a1.getB().add(b2);
      a2.getB().add(b1);
      a2.getB().add(b2);
      a2.getB().add(b3);
      a3.getB().add(b2);
      a3.getB().add(b3);
      a3.getB().add(b4);
      a4.getB().add(b3);
      a4.getB().add(b4);
      a4.getB().add(b5);
      a5.getB().add(b4);
      a5.getB().add(b5);

      assertTrue(a1.getB().contains(b1));
      assertTrue(a1.getB().contains(b2));
      assertTrue(a2.getB().contains(b1));
      assertTrue(a2.getB().contains(b2));
      assertTrue(a2.getB().contains(b3));
      assertTrue(a3.getB().contains(b2));
      assertTrue(a3.getB().contains(b3));
      assertTrue(a3.getB().contains(b4));
      assertTrue(a4.getB().contains(b3));
      assertTrue(a4.getB().contains(b4));
      assertTrue(a4.getB().contains(b5));
      assertTrue(a5.getB().contains(b4));
      assertTrue(a5.getB().contains(b5));
      
      assertTrue(b1.getA().contains(a1));
      assertTrue(b1.getA().contains(a2));
      assertTrue(b2.getA().contains(a1));
      assertTrue(b2.getA().contains(a2));
      assertTrue(b2.getA().contains(a3));
      assertTrue(b3.getA().contains(a2));
      assertTrue(b3.getA().contains(a3));
      assertTrue(b3.getA().contains(a4));
      assertTrue(b4.getA().contains(a3));
      assertTrue(b4.getA().contains(a4));
      assertTrue(b4.getA().contains(a5));
      assertTrue(b5.getA().contains(a4));
      assertTrue(b5.getA().contains(a5));

      // Change:
      a1.getB().add(b3);
      
      // Expected result:
      assertTrue(a1.getB().contains(b1));
      assertTrue(a1.getB().contains(b2));
      assertTrue(a1.getB().contains(b3));
      
      assertTrue(a2.getB().contains(b1));
      assertTrue(a2.getB().contains(b2));
      assertTrue(a2.getB().contains(b3));
      
      assertTrue(a3.getB().contains(b2));
      assertTrue(a3.getB().contains(b3));
      assertTrue(a3.getB().contains(b4));
      
      assertTrue(a4.getB().contains(b3));
      assertTrue(a4.getB().contains(b4));
      assertTrue(a4.getB().contains(b5));
      
      assertTrue(a5.getB().contains(b4));
      assertTrue(a5.getB().contains(b5));
         
      
      assertTrue(b1.getA().contains(a1));
      assertTrue(b1.getA().contains(a2));
      
      assertTrue(b2.getA().contains(a1));
      assertTrue(b2.getA().contains(a2));
      assertTrue(b2.getA().contains(a3));
      
      assertTrue(b3.getA().contains(a1));
      assertTrue(b3.getA().contains(a2));
      assertTrue(b3.getA().contains(a3));
      assertTrue(b3.getA().contains(a4));
      
      assertTrue(b4.getA().contains(a3));
      assertTrue(b4.getA().contains(a4));
      assertTrue(b4.getA().contains(a5));
   
      assertTrue(b5.getA().contains(a4));
      assertTrue(b5.getA().contains(a5));
   }
   
   // a2.getB().remove(b2);
   public void test_a2GetB_removeB2() throws Exception {
      AHome aHome = getAHome();
      BHome bHome = getBHome();

      // Before change:
      A a1 = aHome.create(new Integer(1));
      A a2 = aHome.create(new Integer(2));
      A a3 = aHome.create(new Integer(3));
      A a4 = aHome.create(new Integer(4));
      A a5 = aHome.create(new Integer(5));

      B b1 = bHome.create(new Integer(-1));
      B b2 = bHome.create(new Integer(-2));
      B b3 = bHome.create(new Integer(-3));
      B b4 = bHome.create(new Integer(-4));
      B b5 = bHome.create(new Integer(-5));
      
      a1.getB().add(b1);
      a1.getB().add(b2);
      a2.getB().add(b1);
      a2.getB().add(b2);
      a2.getB().add(b3);
      a3.getB().add(b2);
      a3.getB().add(b3);
      a3.getB().add(b4);
      a4.getB().add(b3);
      a4.getB().add(b4);
      a4.getB().add(b5);
      a5.getB().add(b4);
      a5.getB().add(b5);

      assertTrue(a1.getB().contains(b1));
      assertTrue(a1.getB().contains(b2));
      assertTrue(a2.getB().contains(b1));
      assertTrue(a2.getB().contains(b2));
      assertTrue(a2.getB().contains(b3));
      assertTrue(a3.getB().contains(b2));
      assertTrue(a3.getB().contains(b3));
      assertTrue(a3.getB().contains(b4));
      assertTrue(a4.getB().contains(b3));
      assertTrue(a4.getB().contains(b4));
      assertTrue(a4.getB().contains(b5));
      assertTrue(a5.getB().contains(b4));
      assertTrue(a5.getB().contains(b5));
      
      assertTrue(b1.getA().contains(a1));
      assertTrue(b1.getA().contains(a2));
      assertTrue(b2.getA().contains(a1));
      assertTrue(b2.getA().contains(a2));
      assertTrue(b2.getA().contains(a3));
      assertTrue(b3.getA().contains(a2));
      assertTrue(b3.getA().contains(a3));
      assertTrue(b3.getA().contains(a4));
      assertTrue(b4.getA().contains(a3));
      assertTrue(b4.getA().contains(a4));
      assertTrue(b4.getA().contains(a5));
      assertTrue(b5.getA().contains(a4));
      assertTrue(b5.getA().contains(a5));

      // Change:
      a2.getB().remove(b2);
      
      // Expected result:
      assertTrue(a1.getB().contains(b1));
      assertTrue(a1.getB().contains(b2));
      
      assertTrue(a2.getB().contains(b1));
      assertTrue(!a2.getB().contains(b2));
      assertTrue(a2.getB().contains(b3));
      
      assertTrue(a3.getB().contains(b2));
      assertTrue(a3.getB().contains(b3));
      assertTrue(a3.getB().contains(b4));
      
      assertTrue(a4.getB().contains(b3));
      assertTrue(a4.getB().contains(b4));
      assertTrue(a4.getB().contains(b5));
      
      assertTrue(a5.getB().contains(b4));
      assertTrue(a5.getB().contains(b5));
      
      
      assertTrue(b1.getA().contains(a1));
      assertTrue(b1.getA().contains(a2));
      
      assertTrue(b2.getA().contains(a1));
      assertTrue(!b2.getA().contains(a2));
      assertTrue(b2.getA().contains(a3));
      
      assertTrue(b3.getA().contains(a2));
      assertTrue(b3.getA().contains(a3));
      assertTrue(b3.getA().contains(a4));
      
      assertTrue(b4.getA().contains(a3));
      assertTrue(b4.getA().contains(a4));
      assertTrue(b4.getA().contains(a5));
      
      assertTrue(b5.getA().contains(a4));
      assertTrue(b5.getA().contains(a5));
   }

   public void setUpEJB() throws Exception {
      deleteAllAsAndBs(getAHome(), getBHome());
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



