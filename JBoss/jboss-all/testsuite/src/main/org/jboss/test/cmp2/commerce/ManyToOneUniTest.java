package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import java.util.Iterator;
import javax.naming.InitialContext;
import junit.framework.TestCase;
import net.sourceforge.junitejb.EJBTestCase;

public class ManyToOneUniTest extends EJBTestCase {

   public ManyToOneUniTest(String name) {
      super(name);
   }

   private ProductHome getProductHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (ProductHome) jndiContext.lookup("commerce/Product");
      } catch(Exception e) {
         e.printStackTrace();
         fail("Exception in getProduct: " + e.getMessage());
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

   private Product a1;
   private Product a2;

   private LineItem[] b1x = new LineItem[20];
   private LineItem[] b2x = new LineItem[30];

   public void setUpEJB() throws Exception {
      ProductHome productHome = getProductHome();
      LineItemHome lineItemHome = getLineItemHome();

      // clean out the db
      deleteAllProducts(productHome);
      deleteAllLineItems(lineItemHome);

      // setup the before change part of the test
      beforeChange(productHome, lineItemHome);
   }

   private void beforeChange(ProductHome productHome, LineItemHome lineItemHome)
         throws Exception {

      // Before change:
      a1 = productHome.create();
      a2 = productHome.create();

      for(int i=0; i<b1x.length; i++) {
         b1x[i] = lineItemHome.create();
         b1x[i].setProduct(a1);
      }

      for(int i=0; i<b2x.length; i++) {
         b2x[i] = lineItemHome.create();
         b2x[i].setProduct(a2);
      }

      // (a1.isIdentical(b11.getA())) && ... && (a1.isIdentical(b1n.getA()
      for(int i=0; i<b1x.length; i++) {
         a1.isIdentical(b1x[i].getProduct());
      }

      // (a2.isIdentical(b21.getA())) && ... && (a2.isIdentical(b2m.getA()
      for(int i=0; i<b2x.length; i++) {
         a2.isIdentical(b2x[i].getProduct());
      }
   }

   // b1j.setA(b2k.getA());
   public void test_b1jSetA_b2kGetA() {
      // Change:

      // b1j.setA(b2k.getA());
      int j = b1x.length / 3;
      int k = b2x.length / 2;
      b1x[j].setProduct(b2x[k].getProduct());

      // Expected result:

      // a1.isIdentical(b11.getA())
      // a1.isIdentical(b12.getA())
      // ...
      // a2.isIdentical(b1j.getA())
      // ...
      // a1.isIdentical(b1n.getA())
      for(int i=0; i<b1x.length; i++) {
         if(i != j) {
            assertTrue(a1.isIdentical(b1x[i].getProduct()));
         } else {
            assertTrue(a2.isIdentical(b1x[i].getProduct()));
         }
      }

      // a2.isIdentical(b21.getA())
      // a2.isIdentical(b22.getA())
      // ...
      // a2.isIdentical(b2k.getA())
      // ...
      // a2.isIdentical(b2m.getA())
      for(int i=0; i<b2x.length; i++) {
         assertTrue(a2.isIdentical(b2x[i].getProduct()));
      }
   }

   public void tearDownEJB() throws Exception {
   }

   public void deleteAllProducts(ProductHome productHome) throws Exception {
      // delete all Products
      Iterator currentProducts = productHome.findAll().iterator();
      while(currentProducts.hasNext()) {
         Product p = (Product)currentProducts.next();
         p.remove();
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



