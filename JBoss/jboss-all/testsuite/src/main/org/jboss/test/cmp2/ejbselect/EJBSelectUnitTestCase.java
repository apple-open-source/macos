package org.jboss.test.cmp2.ejbselect;

import java.util.Collection;
import java.util.Iterator;
import javax.naming.InitialContext;
import junit.framework.Test;
import net.sourceforge.junitejb.EJBTestCase;
import org.jboss.test.JBossTestCase;

public class EJBSelectUnitTestCase extends EJBTestCase {

   static org.apache.log4j.Category log =
       org.apache.log4j.Category.getInstance(EJBSelectUnitTestCase.class);

	public static Test suite() throws Exception {
		return JBossTestCase.getDeploySetup(
            EJBSelectUnitTestCase.class, "cmp2-ejbselect.jar");
   }

	public EJBSelectUnitTestCase(String name) {
		super(name);
	}

	private AHome getAHome() {
		try {
			InitialContext jndiContext = new InitialContext();
			
			return (AHome) jndiContext.lookup("cmp2/ejbselect/A"); 
		} catch(Exception e) {
			log.debug("failed", e);
			fail("Exception in getAHome: " + e.getMessage());
		}
		return null;
	}

	private BHome getBHome() {
		try {
			InitialContext jndiContext = new InitialContext();
			
			return (BHome) jndiContext.lookup("cmp2/ejbselect/B"); 
		} catch(Exception e) {
			log.debug("failed", e);
			fail("Exception in getBHome: " + e.getMessage());
		}
		return null;
	}

   private A a1;
   private A a2;
   private B b1;
   private B b2;
   private B b3;
   private B b4;

   public void setUpEJB() throws Exception {
      AHome ahome = getAHome();
      BHome bhome = getBHome();

      a1 = ahome.create("A1");
      Collection bs = a1.getBs();
      b1 = bhome.create("B1", "Alice", true);
      bs.add(b1);
      b2 = bhome.create("B2", "Bob", true);
      bs.add(b2);
      b3 = bhome.create("B3", "Charlie", false);
      bs.add(b3);
      b4 = bhome.create("B4", "Dan", false);
      bs.add(b4);

      a2 = ahome.create("A2");
   }

   public void testReturnedInterface() throws Exception {
      Iterator i = a1.getSomeBs().iterator();
      while(i.hasNext()) {
         Object obj = i.next();
         assertTrue(obj instanceof B);
         B b = (B) obj;
         b.getName();
      }


      i = a1.getSomeBs().iterator();
      while(i.hasNext()) {
         Object obj = i.next();
         assertTrue(obj instanceof B);
         B b = (B) obj;
         b.getName();
      }
   }

   public void testEJBSelectFromEJBHomeMethod() throws Exception {
      AHome ahome = getAHome();
      Collection results = ahome.getSomeBs(a1);
      for(Iterator iterator = results.iterator(); iterator.hasNext(); ) {
         Object obj = iterator.next();
         assertTrue(obj instanceof B);
         B b = (B) obj;
         b.getName();
      }

      assertTrue(results.contains(b1));
      assertTrue(results.contains(b2));
      assertTrue(results.contains(b3));
      assertTrue(results.contains(b4));
      assertEquals(4, results.size());
   }

   public void testGetSomeBxDeclaredSQL() throws Exception {
      AHome ahome = getAHome();
      Collection results = ahome.getSomeBsDeclaredSQL(a1);
      for(Iterator iterator = results.iterator(); iterator.hasNext(); ) {
         Object obj = iterator.next();
         assertTrue(obj instanceof B);
         B b = (B) obj;
         b.getName();
      }

      assertTrue(results.contains(b1));
      assertTrue(results.contains(b2));
      assertTrue(results.contains(b3));
      assertTrue(results.contains(b4));
      assertEquals(4, results.size());
   }

   public void testGetTrue() throws Exception {
      Collection bs = b1.getTrue();

      assertEquals(2, bs.size());
      
      assertTrue(bs.contains(b1));
      assertTrue(bs.contains(b2));
      assertTrue(!bs.contains(b3));
      assertTrue(!bs.contains(b4));

      Iterator i = bs.iterator();
      while(i.hasNext()) {
         B b = (B)i.next();
         assertTrue(b.getBool());
      }
   }

   public void testGetFalse() throws Exception {
      Collection bs = b1.getFalse();

      assertEquals(2, bs.size());
      
      assertTrue(!bs.contains(b1));
      assertTrue(!bs.contains(b2));
      assertTrue(bs.contains(b3));
      assertTrue(bs.contains(b4));

      Iterator i = bs.iterator();
      while(i.hasNext()) {
         B b = (B)i.next();
         assertTrue(!b.getBool());
      }
   }

   public void testGetAWithBs() throws Exception {
      Collection as = a1.getAWithBs();

      assertEquals(1, as.size());
      
      assertTrue(as.contains(a1));
      assertTrue(!as.contains(a2));

      Iterator i = as.iterator();
      while(i.hasNext()) {
         A a = (A)i.next();
         assertTrue(!a.getBs().isEmpty());
      }
   }

   public void tearDownEJB() throws Exception {
      a1.remove();
      a2.remove();
   }
}
