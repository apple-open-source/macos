package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import java.util.Iterator;
import javax.naming.InitialContext;
import junit.framework.TestCase;
import net.sourceforge.junitejb.EJBTestCase;

public class OneToOneUniTest extends EJBTestCase {

   public OneToOneUniTest(String name) {
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

   private AddressHome getAddressHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (AddressHome) jndiContext.lookup("commerce/Address");
      } catch(Exception e) {
         e.printStackTrace();
         fail("Exception in getAddressHome: " + e.getMessage());
      }
      return null;
   }

   private Order a1;
   private Order a2;

   private Address b1;
   private Address b2;

   public void setUpEJB() throws Exception {
      OrderHome orderHome = getOrderHome();
      AddressHome addressHome = getAddressHome();

      // clean out the db
      deleteAllOrders(orderHome);
      deleteAllAddresses(addressHome);

      // setup the before change part of the test
      beforeChange(orderHome, addressHome);
   }

   private void beforeChange(OrderHome orderHome, AddressHome addressHome)
         throws Exception {

      // Before change:
      a1 = orderHome.create();
      a2 = orderHome.create();

      b1 = addressHome.create();
      a1.setShippingAddress(b1);

      b2 = addressHome.create();
      a2.setShippingAddress(b2);

      assertTrue(a1.getShippingAddress().isIdentical(b1));
      assertTrue(a2.getShippingAddress().isIdentical(b2));
   }

   // a1.setB(a2.getB());
   public void test_a1setB_a2getB() {
      // Change:
      // a1.setB(a2.getB());
      a1.setShippingAddress(a2.getShippingAddress());

      // Expected result:
      // (b2.isIdentical(a1.getB())) && (a2.getB() == null)
      assertTrue(b2.isIdentical(a1.getShippingAddress()));
      assertTrue(a2.getShippingAddress() == null);
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

   public void deleteAllAddresses(AddressHome addressHome) throws Exception {
      // delete all Addresses
      Iterator currentAddresses = addressHome.findAll().iterator();
      while(currentAddresses.hasNext()) {
         Address a = (Address)currentAddresses.next();
         a.remove();
      }   
   }
}



