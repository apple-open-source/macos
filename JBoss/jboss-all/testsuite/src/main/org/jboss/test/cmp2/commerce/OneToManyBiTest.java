package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import java.util.Iterator;
import javax.naming.InitialContext;
import junit.framework.TestCase;
import net.sourceforge.junitejb.EJBTestCase;

public class OneToManyBiTest extends EJBTestCase {

   public OneToManyBiTest(String name) {
      super(name);
   }

   private OrderHome getOrderHome() {
      try {
         InitialContext jndiContext = new InitialContext();
         
         return (OrderHome) jndiContext.lookup("commerce/Order");
      } catch(Exception e) {
         e.printStackTrace();
         fail("Exception in getOrder: " + e.getMessage());
      }
      return null;
   }

   private LineItemHome getLineItemHome() {
      try {
         InitialContext jndiContext = new InitialContext();
         
         return (LineItemHome) jndiContext.lookup("commerce/LineItem");
      } catch(Exception e) {
         e.printStackTrace();
         fail("Exception in getLineItemHome: " + e.getMessage());
      }
      return null;
   }

   private Order a1;
   private Order a2;

   private Collection b1;
   private Collection b2;
         
   private LineItem[] b1x = new LineItem[20];
   private LineItem[] b2x = new LineItem[30];
 
   public void setUpEJB() throws Exception {
      OrderHome orderHome = getOrderHome();
      LineItemHome lineItemHome = getLineItemHome();

      // clean out the db
      deleteAllOrders(orderHome);
      deleteAllLineItems(lineItemHome);

      // setup the before change part of the test
      beforeChange(orderHome, lineItemHome);
   }

  private void beforeChange(OrderHome orderHome, LineItemHome lineItemHome) 
         throws Exception {

      // Before change:
      a1 = orderHome.create();
      a2 = orderHome.create();
         
      b1 = a1.getLineItems();
      b2 = a2.getLineItems();
         
      for(int i=0; i<b1x.length; i++) {
         b1x[i] = lineItemHome.create();
         b1.add(b1x[i]);
      }
      
      for(int i=0; i<b2x.length; i++) {
         b2x[i] = lineItemHome.create();
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
   }

   public void setUp() {
      // get the collections again as they are only 
      // valid for the tx length, and were from last tx
      b1 = a1.getLineItems();
      b2 = a2.getLineItems();
   }

   // a1.setB(a2.getB());
   public void test_a1SetB_a2GetB() {
      // Change:
      a1.setLineItems(a2.getLineItems());
         
      // Expected result:
         
      // a2.getB().isEmpty()
      assertTrue(a2.getLineItems().isEmpty());
         
      // b2.isEmpty()
      assertTrue(b2.isEmpty());
         
      // b1 == a1.getB()
      assertTrue(b1 == a1.getLineItems());
         
      // b2 == a2.getB()
      assertTrue(b2 == a2.getLineItems());
         
      // a1.getB().contains(b21)
      // a1.getB().contains(b22)
      // a1.getB().contains(...)         
      // a1.getB().contains(b2m)
      for(int i=0; i<b2x.length; i++) {
         assertTrue(a1.getLineItems().contains(b2x[i]));
      }
      
      // b11.getA() == null
      // b12.getA() == null
      // ....getA() == null
      // b1n.getA() == null
      for(int i=0; i<b1x.length; i++) {
         assertTrue(b1x[i].getOrder() == null);
      }
      
      
      // a1.isIdentical(b21.getA())
      // a1.isIdentical(b22.getA())
      // a1.isIdentical(....getA())
      // a1.isIdentical(b2m.getA()))
      for(int i=0; i<b2x.length; i++) {
         assertTrue(a1.isIdentical(b2x[i].getOrder()));
      }
   }

   // b2m.setA(b1n.getA());
   public void test_b2mSetA_b1nGetA() {
      // Change:
         
      // b2m.setA(b1n.getA());
      b2x[b2x.length-1].setOrder(b1x[b1x.length-1].getOrder());
      
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

      // a1.isIdentical(b11.getA())
      // a1.isIdentical(b12.getA())
      // a1.isIdentical(....getA())
      // a1.isIdentical(b1n.getA())
      for(int i=0; i<b1x.length; i++) {
         assertTrue(a1.isIdentical(b1x[i].getOrder()));
      }
      
      // a2.isIdentical(b21.getA())
      // a2.isIdentical(b22.getA())
      // a2.isIdentical(....getA())
      // a2.isIdentical(b2m_1.getA())
      for(int i=0; i<b2x.length-1; i++) {
         assertTrue(a2.isIdentical(b2x[i].getOrder()));
      }
      
      // a1.isIdentical(b2m.getA())
      assertTrue(a1.isIdentical(b2x[b2x.length-1].getOrder()));
   }
   
   // a1.getB().add(b2m);
   public void test_a1GetB_addB2m() {
      // Change:
         
      // a1.getB().add(b2m);
      a1.getLineItems().add(b2x[b2x.length-1]);
         
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

      // a1.isIdentical(b11.getA())
      // a1.isIdentical(b12.getA())
      // a1.isIdentical(....getA())
      // a1.isIdentical(b1n.getA())
      for(int i=0; i<b1x.length; i++) {
         assertTrue(a1.isIdentical(b1x[i].getOrder()));
      }
      
      // a2.isIdentical(b21.getA())
      // a2.isIdentical(b22.getA())
      // a2.isIdentical(....getA())
      // a2.isIdentical(b2m_1.getA())
      for(int i=0; i<b2x.length-1; i++) {
         assertTrue(a2.isIdentical(b2x[i].getOrder()));
      }
      
      // a1.isIdentical(b2m.getA())
      assertTrue(a1.isIdentical(b2x[b2x.length-1].getOrder()));
   }
   
   // a1.getB().remove(b1n);
   public void test_a1GetB_removeB1n() {
      // Change:
      
      // a1.getB().remove(b1n);
      a1.getLineItems().remove(b1x[b1x.length-1]);
      
      // Expected result:
      
      // b1n.getA() == null
      assertTrue(b1x[b1x.length-1].getOrder() == null);
      
      // b1 == a1.getB()
      assertTrue(b1 == a1.getLineItems());
      
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

   public void tearDownEJB() throws Exception {
   }
   
   public void deleteAllOrders(OrderHome orderHome) throws Exception {
      // delete all Orders
      Iterator currentOrders = orderHome.findAll().iterator();
      while(currentOrders.hasNext()) {
         Order o = (Order)currentOrders.next();
         o.remove();
      }   
   }

   public void deleteAllLineItems(LineItemHome lineItemHome) throws Exception {
      // delete all LineItems
      Iterator currentLineItems = lineItemHome.findAll().iterator();
      while(currentLineItems.hasNext()) {
         LineItem l = (LineItem)currentLineItems.next();
         l.remove();
      }   
   }
}



