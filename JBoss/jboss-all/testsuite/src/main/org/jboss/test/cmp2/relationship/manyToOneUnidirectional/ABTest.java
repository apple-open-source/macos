package org.jboss.test.cmp2.relationship.manyToOneUnidirectional;

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
         return (AHome) jndiContext.lookup("relation/manyToOne/unidirectional/table/A"); 
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getTableAHome: " + e.getMessage());
      }
      return null;
   }

   private BHome getTableBHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (BHome) jndiContext.lookup("relation/manyToOne/unidirectional/table/B");
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getTableBHome: " + e.getMessage());
      }
      return null;
   }

   private AHome getFKAHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (AHome) jndiContext.lookup("relation/manyToOne/unidirectional/fk/A");
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getFKAHome: " + e.getMessage());
      }
      return null;
   }

   private BHome getFKBHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (BHome) jndiContext.lookup("relation/manyToOne/unidirectional/fk/B");
      } catch(Exception e) {
         log.debug("failed", e);
         fail("Exception in getFKBHome: " + e.getMessage());
      }
      return null;
   }

   // b1j.setA(b2k.getA());
   public void test_b1jSetA_b2kGetA_Table() throws Exception {
      AHome aHome = getTableAHome();
      BHome bHome = getTableBHome();
      b1jSetA_b2kGetA(aHome, bHome);
   }

   // b1j.setA(b2k.getA());
   public void test_b1jSetA_b2kGetA_FK() throws Exception {
      AHome aHome = getFKAHome();
      BHome bHome = getFKBHome();
      b1jSetA_b2kGetA(aHome, bHome);
   }

   // b1j.setA(b2k.getA());
   private void b1jSetA_b2kGetA(AHome aHome, BHome bHome) throws Exception {
      // Before change:
      A a1 = aHome.create(new Integer(1));
      A a2 = aHome.create(new Integer(2));
      
      B[] b1x = new B[20];
      B[] b2x = new B[30];
      
      for(int i=0; i<b1x.length; i++) {
         b1x[i] = bHome.create(new Integer(10000 + i));
         b1x[i].setA(a1);
      }
      
      for(int i=0; i<b2x.length; i++) {
         b2x[i] = bHome.create(new Integer(20000 + i));
         b2x[i].setA(a2);
      }
      
      // (a1.isIdentical(b11.getA())) && ... && (a1.isIdentical(b1n.getA()
      for(int i=0; i<b1x.length; i++) {
         a1.isIdentical(b1x[i].getA());
      }
      
      // (a2.isIdentical(b21.getA())) && ... && (a2.isIdentical(b2m.getA()
      for(int i=0; i<b2x.length; i++) {
         a2.isIdentical(b2x[i].getA());
      }
      
      // Change:
      
      // b1j.setA(b2k.getA());
      int j = b1x.length / 3;
      int k = b2x.length / 2;
      b1x[j].setA(b2x[k].getA());
      
      // Expected result:
      
      // a1.isIdentical(b11.getA())
      // a1.isIdentical(b12.getA())
      // ...
      // a2.isIdentical(b1j.getA())
      // ...
      // a1.isIdentical(b1n.getA())
      for(int i=0; i<b1x.length; i++) {
         if(i != j) {
            assertTrue(a1.isIdentical(b1x[i].getA()));
         } else {
            assertTrue(a2.isIdentical(b1x[i].getA()));
         }
      }

      // a2.isIdentical(b21.getA())
      // a2.isIdentical(b22.getA())
      // ...
      // a2.isIdentical(b2k.getA())
      // ...
      // a2.isIdentical(b2m.getA())
      for(int i=0; i<b2x.length; i++) {
         assertTrue(a2.isIdentical(b2x[i].getA()));
      }
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



