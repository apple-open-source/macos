package org.jboss.test.cmp2.commerce;

import java.util.*;
import junit.framework.*;
import javax.ejb.*;
import javax.naming.*;
import javax.rmi.PortableRemoteObject;

public class UserTest extends TestCase {

   public UserTest(String name) {
      super(name);
   }

   private UserHome getUserHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         Object ref   = jndiContext.lookup("commerce/User");
         return (UserHome)PortableRemoteObject.narrow (ref, UserHome.class);
      } catch(Exception e) {
         fail("Exception in getUserHome: " + e.getMessage());
      }
      return null;
   }

   private CustomerHome getCustomerHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         Object ref   = jndiContext.lookup("commerce/Customer");

         // Get a reference from this to the Bean's Home interface
         return (CustomerHome)PortableRemoteObject.narrow(
               ref, CustomerHome.class);
      } catch(Exception e) {
         fail("Exception in getCustomerHome: " + e.getMessage());
      }
      return null;
   }

   public void testUserQueries() throws Exception {
      UserHome home = getUserHome();
      home.findAllByUserName("Test User 1");
   }

   public void testAddCd() {
   /*
      try {
         UserHome userHome = getUserHome();

         String userId = "dain";
         String userName = "Dain Sundstrom";
         String email = "dain@daingroup.com";

         User user = userHome.create(userId);
         user.setUserName(userName);
         user.setEmail(email);
         user.setSendSpam(false);

         assertEquals(userId, user.getUserId());
         assertEquals(userName, user.getUserName());
         assertEquals(email, user.getEmail());
         assertEquals(false, user.getSendSpam());

         user = null;

         user = userHome.findByUserName(userName);
         assertEquals(userId, user.getUserId());
         assertEquals(userName, user.getUserName());
         assertEquals(email, user.getEmail());
         assertEquals(false, user.getSendSpam());

         // test customer -> user relationship
         CustomerHome customerHome = getCustomerHome();
         Customer customer = customerHome.create(new Long(random.nextLong()));
         customer.setName("The Daingroup, LLC.");
         customer.setUser(user);

         // is the user correct
         assertTrue(user.isIdentical(customer.getUser()));

         user = customer.getUser();
         assertEquals(userId, user.getUserId());
         assertEquals(userName, user.getUserName());
         assertEquals(email, user.getEmail());
         assertEquals(false, user.getSendSpam());

         // set cutomer to null and see if it stays null
         customer.setUser(null);
         assertNull(customer.getUser());

         // reset user
         customer.setUser(user);

         // removing user
         userHome.remove(user.getPrimaryKey());

         try {
            user = userHome.findByUserName(userName);
            fail("should throw ObjectNotFoundException");
         } catch(ObjectNotFoundException e) {
         }

         // user was deleted so customer's user ref should be null
         assertNull(customer.getUser());

         customer.remove();
      } catch(Exception e) {
         e.printStackTrace();
         fail("Error in big old method: ");
      }
*/
   }
   public static Test suite() {
      return new TestSuite(UserTest.class);
   }
}



