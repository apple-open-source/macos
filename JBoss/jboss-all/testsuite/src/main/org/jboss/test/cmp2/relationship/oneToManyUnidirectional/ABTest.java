package org.jboss.test.cmp2.relationship.oneToManyUnidirectional;

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

         return (AHome) jndiContext.lookup("relation/oneToMany/unidirectional/table/A");
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getTableAHome: " + e.getMessage());
      }
      return null;
   }

   private BHome getTableBHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (BHome) jndiContext.lookup("relation/oneToMany/unidirectional/table/B");
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getTableBHome: " + e.getMessage());
      }
      return null;
   }

   private AHome getFKAHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (AHome) jndiContext.lookup("relation/oneToMany/unidirectional/fk/A");
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getFKAHome: " + e.getMessage());
      }
      return null;
   }

   private BHome getFKBHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (BHome) jndiContext.lookup("relation/oneToMany/unidirectional/fk/B");
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getFKBHome: " + e.getMessage());
      }
      return null;
   }

   // a1.setB(a2.getB());
   public void Xtest_a1SetB_a2GetB_Table() throws Exception {
      AHome aHome = getTableAHome();
      BHome bHome = getTableBHome();
      a1SetB_a2GetB(aHome, bHome);
   }

   // a1.setB(a2.getB());
   public void test_a1SetB_a2GetB_FK() throws Exception {
      AHome aHome = getFKAHome();
      BHome bHome = getFKBHome();
      a1SetB_a2GetB(aHome, bHome);
   }

   // a1.setB(a2.getB());
   private void a1SetB_a2GetB(AHome aHome, BHome bHome) throws Exception {
      // Before change:
      A a1 = aHome.create(new Integer(1));
      A a2 = aHome.create(new Integer(2));
      
      Collection b1 = a1.getB();
      a1.getId();
      assertTrue(b1.isEmpty());

      Collection b2 = a2.getB();
      a2.getId();
      assertTrue(b2.isEmpty());
      
      B[] b1x = new B[20];
      B[] b2x = new B[30];
      
      for(int i=0; i<b1x.length; i++) {
         b1x[i] = bHome.create(new Integer(10000 + i));
         b1.add(b1x[i]);
      }
      
      for(int i=0; i<b2x.length; i++) {
         b2x[i] = bHome.create(new Integer(20000 + i));
         b2.add(b2x[i]);
      }
      
      // B b11, b12, ... , b1n; members of b1
      for(int i=0; i<b1x.length; i++) {
         assertTrue(b1.contains(b1x[i]));
      }
      
      // B b21, b22, ... , b2m; members of b2
      for(int i=0; i<b2x.length; i++) {
         assertTrue(b2.contains(b2x[i]));
      }
      
      // Change:
      a1.setB(a2.getB());
      
      // Expected result:
      
      // a2.getB().isEmpty()
      assertTrue(a2.getB().isEmpty());
      
      // b2.isEmpty()
      assertTrue(b2.isEmpty());
      
      // b1 == a1.getB()
      assertTrue(b1 == a1.getB());
      
      // b2 == a2.getB()
      assertTrue(b2 == a2.getB());
      
      // a1.getB().contains(b21)
      // a1.getB().contains(b22)
      // a1.getB().contains(...)         
      // a1.getB().contains(b2m)
      for(int i=0; i<b2x.length; i++) {
         assertTrue(a1.getB().contains(b2x[i]));
      }
   }
   
   // a1.getB().add(b2m);
   public void Xtest_a1GetB_addB2m_Table() throws Exception {
      AHome aHome = getTableAHome();
      BHome bHome = getTableBHome();
      a1GetB_addB2m(aHome, bHome);
   }

   // a1.getB().add(b2m);
   public void Xtest_a1GetB_addB2m_FK() throws Exception {
      AHome aHome = getFKAHome();
      BHome bHome = getFKBHome();
      a1GetB_addB2m(aHome, bHome);
   }

   // a1.getB().add(b2m);
   private void a1GetB_addB2m(AHome aHome, BHome bHome) throws Exception {
      // Before change:
      A a1 = aHome.create(new Integer(1));
      A a2 = aHome.create(new Integer(2));
      
      Collection b1 = a1.getB();
      Collection b2 = a2.getB();
      
      B[] b1x = new B[20];
      B[] b2x = new B[30];
      
      for(int i=0; i<b1x.length; i++) {
         b1x[i] = bHome.create(new Integer(10000 + i));
         b1.add(b1x[i]);
      }
      
      for(int i=0; i<b2x.length; i++) {
         b2x[i] = bHome.create(new Integer(20000 + i));
         b2.add(b2x[i]);
      }
      
      // B b11, b12, ... , b1n; members of b1
      for(int i=0; i<b1x.length; i++) {
         assertTrue(b1.contains(b1x[i]));
      }
      
      // B b21, b22, ... , b2m; members of b2
      for(int i=0; i<b2x.length; i++) {
         assertTrue(b2.contains(b2x[i]));
      }
      
      // Change:
      
      // a1.getB().add(b2m);
      a1.getB().add(b2x[b2x.length-1]);
         
      // Expected result:
   
      // b1.contains(b11)
      // b1.contains(b12)
      // b1.contains(...)
      // b1.contains(b1n)
      for(int i=0; i<b1x.length; i++) {
         assertTrue(b1.contains(b1x[i]));
      }

      // b1.contains(b2m)
      assertTrue(b1.contains(b2x[b2x.length-1]));

      // b2.contains(b21)
      // b2.contains(b22)
      // b2.contains(...)
      // b2.contains(b2m_1)
      for(int i=0; i<b2x.length-1; i++) {
         assertTrue(b2.contains(b2x[i]));
      }
   }
   
   // a1.getB().remove(b1n);
   public void Xtest_a1GetB_removeB1n_Table() throws Exception {
      AHome aHome = getTableAHome();
      BHome bHome = getTableBHome();
      a1GetB_removeB1n(aHome, bHome);
   }

   // a1.getB().remove(b1n);
   public void Xtest_a1GetB_removeB1n_FK() throws Exception {
      AHome aHome = getFKAHome();
      BHome bHome = getFKBHome();
      a1GetB_removeB1n(aHome, bHome);
   }

   // a1.getB().remove(b1n);
   private void a1GetB_removeB1n(AHome aHome, BHome bHome) throws Exception {
      // Before change:
      A a1 = aHome.create(new Integer(1));
      A a2 = aHome.create(new Integer(2));
      
      Collection b1 = a1.getB();
      Collection b2 = a2.getB();
      
      B[] b1x = new B[20];
      B[] b2x = new B[30];
      
      for(int i=0; i<b1x.length; i++) {
         b1x[i] = bHome.create(new Integer(10000 + i));
         b1.add(b1x[i]);
      }
      
      for(int i=0; i<b2x.length; i++) {
         b2x[i] = bHome.create(new Integer(20000 + i));
         b2.add(b2x[i]);
      }
         
      // B b11, b12, ... , b1n; members of b1
      for(int i=0; i<b1x.length; i++) {
         assertTrue(b1.contains(b1x[i]));
      }
      
      // B b21, b22, ... , b2m; members of b2
      for(int i=0; i<b2x.length; i++) {
         assertTrue(b2.contains(b2x[i]));
      }
      
      // Change:
      
      // a1.getB().remove(b1n);
      a1.getB().remove(b1x[b1x.length-1]);
      
      // Expected result:
      
      // b1 == a1.getB()
      assertTrue(b1 == a1.getB());
         
      // b1.contains(b11)
      // b1.contains(b12)
      // b1.contains(...)
      // b1.contains(b1n_1)
      for(int i=0; i<b1x.length-1; i++) {
         assertTrue(b1.contains(b1x[i]));
      }

      // !(b1.contains(b1n))
      assertTrue(!(b1.contains(b1x[b1x.length-1])));
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



