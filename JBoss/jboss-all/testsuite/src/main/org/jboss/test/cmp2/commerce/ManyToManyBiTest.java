package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import java.util.Iterator;
import javax.naming.InitialContext;
import junit.framework.TestCase;
import net.sourceforge.junitejb.EJBTestCase;

public class ManyToManyBiTest extends EJBTestCase {

   public ManyToManyBiTest(String name) {
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

   private ProductCategoryHome getProductCategoryHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (ProductCategoryHome)
               jndiContext.lookup("commerce/ProductCategory");
      } catch(Exception e) {
         e.printStackTrace();
         fail("Exception in getProductCategory: " + e.getMessage());
      }
      return null;
   }

   private Product a1;
   private Product a2;
   private Product a3;
   private Product a4;
   private Product a5;

   private ProductCategory b1;
   private ProductCategory b2;
   private ProductCategory b3;
   private ProductCategory b4;
   private ProductCategory b5;

   public void setUpEJB() throws Exception {
      ProductHome productHome = getProductHome();
      ProductCategoryHome productCategoryHome = getProductCategoryHome();

      // clean out the db
      deleteAllProducts(productHome);
      deleteAllProductCategories(productCategoryHome);

      // setup the before change part of the test
      beforeChange(productHome, productCategoryHome);
   }

   private void beforeChange(
         ProductHome productHome,
         ProductCategoryHome productCategoryHome) throws Exception {

      // Before change:
      a1 = productHome.create();
      a2 = productHome.create();
      a3 = productHome.create();
      a4 = productHome.create();
      a5 = productHome.create();

      b1 = productCategoryHome.create();
      b2 = productCategoryHome.create();
      b3 = productCategoryHome.create();
      b4 = productCategoryHome.create();
      b5 = productCategoryHome.create();

      a1.getProductCategories().add(b1);
      a1.getProductCategories().add(b2);
      a2.getProductCategories().add(b1);
      a2.getProductCategories().add(b2);
      a2.getProductCategories().add(b3);
      a3.getProductCategories().add(b2);
      a3.getProductCategories().add(b3);
      a3.getProductCategories().add(b4);
      a4.getProductCategories().add(b3);
      a4.getProductCategories().add(b4);
      a4.getProductCategories().add(b5);
      a5.getProductCategories().add(b4);
      a5.getProductCategories().add(b5);

      assertTrue(a1.getProductCategories().contains(b1));
      assertTrue(a1.getProductCategories().contains(b2));
      assertTrue(a2.getProductCategories().contains(b1));
      assertTrue(a2.getProductCategories().contains(b2));
      assertTrue(a2.getProductCategories().contains(b3));
      assertTrue(a3.getProductCategories().contains(b2));
      assertTrue(a3.getProductCategories().contains(b3));
      assertTrue(a3.getProductCategories().contains(b4));
      assertTrue(a4.getProductCategories().contains(b3));
      assertTrue(a4.getProductCategories().contains(b4));
      assertTrue(a4.getProductCategories().contains(b5));
      assertTrue(a5.getProductCategories().contains(b4));
      assertTrue(a5.getProductCategories().contains(b5));

      assertTrue(b1.getProducts().contains(a1));
      assertTrue(b1.getProducts().contains(a2));
      assertTrue(b2.getProducts().contains(a1));
      assertTrue(b2.getProducts().contains(a2));
      assertTrue(b2.getProducts().contains(a3));
      assertTrue(b3.getProducts().contains(a2));
      assertTrue(b3.getProducts().contains(a3));
      assertTrue(b3.getProducts().contains(a4));
      assertTrue(b4.getProducts().contains(a3));
      assertTrue(b4.getProducts().contains(a4));
      assertTrue(b4.getProducts().contains(a5));
      assertTrue(b5.getProducts().contains(a4));
      assertTrue(b5.getProducts().contains(a5));
   }


   // a1.setB(a3.getB());
   public void test_a1SetB_a3GetB() {
      // Change:
      a1.setProductCategories(a3.getProductCategories());

      // Expected result:
      assertTrue(!a1.getProductCategories().contains(b1));
      assertTrue(a1.getProductCategories().contains(b2));
      assertTrue(a1.getProductCategories().contains(b3));
      assertTrue(a1.getProductCategories().contains(b4));

      assertTrue(a2.getProductCategories().contains(b1));
      assertTrue(a2.getProductCategories().contains(b2));
      assertTrue(a2.getProductCategories().contains(b3));

      assertTrue(a3.getProductCategories().contains(b2));
      assertTrue(a3.getProductCategories().contains(b3));
      assertTrue(a3.getProductCategories().contains(b4));

      assertTrue(a4.getProductCategories().contains(b3));
      assertTrue(a4.getProductCategories().contains(b4));
      assertTrue(a4.getProductCategories().contains(b5));

      assertTrue(a5.getProductCategories().contains(b4));
      assertTrue(a5.getProductCategories().contains(b5));


      assertTrue(!b1.getProducts().contains(a1));
      assertTrue(b1.getProducts().contains(a2));

      assertTrue(b2.getProducts().contains(a1));
      assertTrue(b2.getProducts().contains(a2));
      assertTrue(b2.getProducts().contains(a3));

      assertTrue(b3.getProducts().contains(a1));
      assertTrue(b3.getProducts().contains(a2));
      assertTrue(b3.getProducts().contains(a3));
      assertTrue(b3.getProducts().contains(a4));

      assertTrue(b4.getProducts().contains(a1));
      assertTrue(b4.getProducts().contains(a3));
      assertTrue(b4.getProducts().contains(a4));
      assertTrue(b4.getProducts().contains(a5));

      assertTrue(b5.getProducts().contains(a4));
      assertTrue(b5.getProducts().contains(a5));
   }

   // a1.getB().add(b3);
   public void test_a1GetB_addB3() {
      // Change:
      a1.getProductCategories().add(b3);

      // Expected result:
      assertTrue(a1.getProductCategories().contains(b1));
      assertTrue(a1.getProductCategories().contains(b2));
      assertTrue(a1.getProductCategories().contains(b3));

      assertTrue(a2.getProductCategories().contains(b1));
      assertTrue(a2.getProductCategories().contains(b2));
      assertTrue(a2.getProductCategories().contains(b3));

      assertTrue(a3.getProductCategories().contains(b2));
      assertTrue(a3.getProductCategories().contains(b3));
      assertTrue(a3.getProductCategories().contains(b4));

      assertTrue(a4.getProductCategories().contains(b3));
      assertTrue(a4.getProductCategories().contains(b4));
      assertTrue(a4.getProductCategories().contains(b5));

      assertTrue(a5.getProductCategories().contains(b4));
      assertTrue(a5.getProductCategories().contains(b5));


      assertTrue(b1.getProducts().contains(a1));
      assertTrue(b1.getProducts().contains(a2));

      assertTrue(b2.getProducts().contains(a1));
      assertTrue(b2.getProducts().contains(a2));
      assertTrue(b2.getProducts().contains(a3));

      assertTrue(b3.getProducts().contains(a1));
      assertTrue(b3.getProducts().contains(a2));
      assertTrue(b3.getProducts().contains(a3));
      assertTrue(b3.getProducts().contains(a4));

      assertTrue(b4.getProducts().contains(a3));
      assertTrue(b4.getProducts().contains(a4));
      assertTrue(b4.getProducts().contains(a5));

      assertTrue(b5.getProducts().contains(a4));
      assertTrue(b5.getProducts().contains(a5));
   }

   // a2.getB().remove(b2);
   public void test_a2GetB_removeB2() {
      // Change:
      a2.getProductCategories().remove(b2);

      // Expected result:
      assertTrue(a1.getProductCategories().contains(b1));
      assertTrue(a1.getProductCategories().contains(b2));

      assertTrue(a2.getProductCategories().contains(b1));
      assertTrue(!a2.getProductCategories().contains(b2));
      assertTrue(a2.getProductCategories().contains(b3));

      assertTrue(a3.getProductCategories().contains(b2));
      assertTrue(a3.getProductCategories().contains(b3));
      assertTrue(a3.getProductCategories().contains(b4));

      assertTrue(a4.getProductCategories().contains(b3));
      assertTrue(a4.getProductCategories().contains(b4));
      assertTrue(a4.getProductCategories().contains(b5));

      assertTrue(a5.getProductCategories().contains(b4));
      assertTrue(a5.getProductCategories().contains(b5));


      assertTrue(b1.getProducts().contains(a1));
      assertTrue(b1.getProducts().contains(a2));

      assertTrue(b2.getProducts().contains(a1));
      assertTrue(!b2.getProducts().contains(a2));
      assertTrue(b2.getProducts().contains(a3));

      assertTrue(b3.getProducts().contains(a2));
      assertTrue(b3.getProducts().contains(a3));
      assertTrue(b3.getProducts().contains(a4));

      assertTrue(b4.getProducts().contains(a3));
      assertTrue(b4.getProducts().contains(a4));
      assertTrue(b4.getProducts().contains(a5));

      assertTrue(b5.getProducts().contains(a4));
      assertTrue(b5.getProducts().contains(a5));
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

   public void deleteAllProductCategories(ProductCategoryHome productCategoryHome) throws Exception {
      // delete all ProductCategories
      Iterator currentProductCategories = productCategoryHome.findAll().iterator();
      while(currentProductCategories.hasNext()) {
         ProductCategory pc = (ProductCategory)currentProductCategories.next();
         pc.remove();
      }   
   }


}



