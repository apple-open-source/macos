package org.jboss.test.cmp2.commerce;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.Set;
import javax.naming.InitialContext;
import junit.framework.Test;
import net.sourceforge.junitejb.EJBTestCase;

import org.jboss.test.JBossTestCase;

public class CommerceTest extends EJBTestCase {
   public static Test suite() throws Exception {
		return JBossTestCase.getDeploySetup(
            CommerceTest.class, 
            "cmp2-commerce.jar");
   }   

   public CommerceTest(String name) {
      super(name);
   }

   private OrderHome getOrderHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (OrderHome) jndiContext.lookup("commerce/Order"); 
      } catch(Exception e) {
         e.printStackTrace();
         fail("Exception in getOrderHome: " + e.getMessage());
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

   private ProductHome getProductHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (ProductHome) jndiContext.lookup("commerce/Product"); 
      } catch(Exception e) {
         e.printStackTrace();
         fail("Exception in getProductHome: " + e.getMessage());
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

   public void test_setInPostCreate() throws Exception {

      OrderHome oh = getOrderHome();
      LineItemHome lh = getLineItemHome();

      LineItem l =lh.create();
      
      Order o = oh.create();
      l = lh.create(o);
      assertTrue(l.getOrder().isIdentical(o));
   }

   public void test_dvo() throws Exception {
      OrderHome orderHome = getOrderHome();

      Order order = orderHome.create();
      Long ordernumber = order.getOrdernumber();

      // setup credit card
      Card creditCard = new Card();
      creditCard.setType(Card.VISA);
      creditCard.setCardNumber("1111-2222-3333-4444");
      creditCard.setCardHolder(new FormalName("Dain", 'S', "Sundstrom"));
      creditCard.setBillingZip(55414);

      order.setCreditCard(creditCard);
      assertEquals(order.getCreditCard(), creditCard);

      order = null;
      order = orderHome.findByPrimaryKey(ordernumber);
      assertEquals(order.getCreditCard(), creditCard);
   }

   public void test_getOrdersShippedToCA() throws Exception {
      OrderHome orderHome = getOrderHome();
      AddressHome addressHome = getAddressHome();
      
      Order orderCA1 = orderHome.create();
      Address shipCA1 = addressHome.create();
      shipCA1.setState("CA");
      orderCA1.setShippingAddress(shipCA1);
      
      Order orderCA2 = orderHome.create();
      Address shipCA2 = addressHome.create();
      shipCA2.setState("CA");
      orderCA2.setShippingAddress(shipCA2);

      Order orderMN = orderHome.create();
      Address shipMN = addressHome.create();
      shipMN.setState("MN");
      orderMN.setShippingAddress(shipMN);

      Set s = orderMN.getOrdersShippedToCA();
      System.out.println(s);
      assertTrue(s.contains(orderCA1));
      assertTrue(s.contains(orderCA2));
      assertTrue(!s.contains(orderMN));
      assertTrue(s.size()==2);

      s = orderMN.getOrdersShippedToCA2();
      System.out.println(s);
      assertTrue(s.contains(orderCA1));
      assertTrue(s.contains(orderCA2));
      assertTrue(!s.contains(orderMN));
      assertTrue(s.size()==2);
   }   

   public void test_getStatesShipedTo() throws Exception {
      OrderHome orderHome = getOrderHome();
      AddressHome addressHome = getAddressHome();
      
      Order orderCA1 = orderHome.create();
      Address shipCA1 = addressHome.create();
      shipCA1.setState("CA");
      orderCA1.setShippingAddress(shipCA1);
      
      Order orderCA2 = orderHome.create();
      Address shipCA2 = addressHome.create();
      shipCA2.setState("CA");
      orderCA2.setShippingAddress(shipCA2);

      Order orderMN = orderHome.create();
      Address shipMN = addressHome.create();
      shipMN.setState("MN");
      orderMN.setShippingAddress(shipMN);

      System.out.println("orderMN.getStatesShipedTo();");
      Collection c = orderMN.getStatesShipedTo();
      System.out.println(c);
      assertTrue(c.contains("CA"));
      assertTrue(c.contains("MN"));
      assertTrue(c.size()==3);

      c = orderMN.getStatesShipedTo2();
      System.out.println(c);
      assertTrue(c.contains("CA"));
      assertTrue(c.contains("MN"));
      assertTrue(c.size()==3);
   }   

   public void test_getAddressesInCA() throws Exception {
      OrderHome orderHome = getOrderHome();
      AddressHome addressHome = getAddressHome();
      
      Order orderCA1 = orderHome.create();
      Address shipCA1 = addressHome.create();
      shipCA1.setState("CA");
      orderCA1.setShippingAddress(shipCA1);
      Address billCA1 = addressHome.create();
      billCA1.setState("CA");
      orderCA1.setBillingAddress(billCA1);

      Order orderCA2 = orderHome.create();
      Address shipCA2 = addressHome.create();
      shipCA2.setState("CA");
      orderCA2.setShippingAddress(shipCA2);
      orderCA2.setBillingAddress(shipCA2);

      Order orderMN = orderHome.create();
      Address shipMN = addressHome.create();
      shipMN.setState("MN");
      orderMN.setShippingAddress(shipMN);

      Collection c = orderMN.getAddressesInCA();
      System.out.println(c);
      assertTrue(c.contains(shipCA1));
      assertTrue(c.contains(shipCA2));
      assertTrue(c.contains(billCA1));
      assertTrue(c.size()==3);

      c = orderMN.getAddressesInCA2();
      System.out.println(c);
      assertTrue(c.contains(shipCA1));
      assertTrue(c.contains(shipCA2));
      assertTrue(c.contains(billCA1));
      assertTrue(c.size()==3);
   }   


   public void test_findDoubleJoin() throws Exception {
      OrderHome orderHome = getOrderHome();
      LineItemHome lineItemHome = getLineItemHome();
      
      Order order1 = orderHome.create();
      LineItem line1 = lineItemHome.create();
      line1.setQuantity(1);
      order1.getLineItems().add(line1);
      LineItem line2 = lineItemHome.create();
      line2.setQuantity(2);
      order1.getLineItems().add(line2);

      Order order2 = orderHome.create();
      LineItem line3 = lineItemHome.create();
      line3.setQuantity(2);
      order2.getLineItems().add(line3);

      
      Collection c = orderHome.findDoubleJoin(1, 2);
      assertTrue(c.contains(order1));
      assertTrue(!c.contains(order2));
      assertTrue(c.size()==1);
   }   

   public void testCMRReset() throws Exception {
      OrderHome orderHome = getOrderHome();
      LineItemHome lineItemHome = getLineItemHome();
      
      Order order = orderHome.create();
      Collection lineItems = order.getLineItems();

      LineItem line1 = lineItemHome.create();
      line1.setQuantity(1);
      lineItems.add(line1);

      LineItem line2 = lineItemHome.create();
      line2.setQuantity(2);
      lineItems.add(line2);

      LineItem line3 = lineItemHome.create();
      line3.setQuantity(2);
      lineItems.add(line3);

      
      assertEquals(lineItems.size(), 3);
      assertTrue(lineItems == order.getLineItems());
      order.setLineItems(lineItems);
      assertEquals(lineItems.size(), 3);
      assertTrue(lineItems == order.getLineItems());
   }   

   public void testCMRSetFromNewCollection() throws Exception {
      OrderHome orderHome = getOrderHome();
      LineItemHome lineItemHome = getLineItemHome();
      
      Order order = orderHome.create();
      Collection lineItems = new ArrayList();

      LineItem line1 = lineItemHome.create();
      line1.setQuantity(1);
      lineItems.add(line1);

      LineItem line2 = lineItemHome.create();
      line2.setQuantity(2);
      lineItems.add(line2);

      LineItem line3 = lineItemHome.create();
      line3.setQuantity(2);
      lineItems.add(line3);

      
      assertEquals(lineItems.size(), 3);
      order.setLineItems(lineItems);
      assertEquals(lineItems.size(), 3);
      assertEquals(order.getLineItems().size(), 3);
   }

   public void testIsIdentical() throws Exception
   {
      OrderHome orderHome = getOrderHome();
      Order order = orderHome.create(new Long(111));

      LineItemHome liHome = getLineItemHome();
      LineItem lineItem = liHome.create(new Long(111));

      assertTrue("!order.isIdentical(lineItem)", !order.isIdentical(lineItem));
      assertTrue("order.isIdentical(order)", order.isIdentical(order));
   }

   public void setUpEJB() throws Exception {
      deleteAllOrders(getOrderHome());
      deleteAllLineItems(getLineItemHome());
      deleteAllProducts(getProductHome());
      deleteAllAddresses(getAddressHome());
   }
   
   public void tearDownEJB() throws Exception {
      deleteAllOrders(getOrderHome());
      deleteAllLineItems(getLineItemHome());
      deleteAllProducts(getProductHome());
      deleteAllAddresses(getAddressHome());
   }
   
   public void deleteAllOrders(OrderHome orderHome) throws Exception {
      Iterator orders = orderHome.findAll().iterator();
      while(orders.hasNext()) {
         Order order = (Order)orders.next();
         order.remove();
      }   
   }

   public void deleteAllLineItems(LineItemHome lineItemHome) throws Exception {
      Iterator lineItems = lineItemHome.findAll().iterator();
      while(lineItems.hasNext()) {
         LineItem lineItem = (LineItem)lineItems.next();
         lineItem.remove();
      }   
   }

   public void deleteAllProducts(ProductHome productHome) throws Exception {
      Iterator products = productHome.findAll().iterator();
      while(products.hasNext()) {
         Product product = (Product)products.next();
         product.remove();
      }   
   }

   public void deleteAllAddresses(AddressHome addressHome) throws Exception {
      Iterator addresses = addressHome.findAll().iterator();
      while(addresses.hasNext()) {
         Address address = (Address)addresses.next();
         address.remove();
      }   
   }
}
