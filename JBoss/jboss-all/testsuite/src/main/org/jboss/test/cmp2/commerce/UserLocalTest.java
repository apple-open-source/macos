package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import java.util.Iterator;
import javax.naming.InitialContext;
import junit.framework.TestCase;
import net.sourceforge.junitejb.EJBTestCase;

public class UserLocalTest extends EJBTestCase {

   public UserLocalTest(String name) {
      super(name);
   }

   private UserLocalHome getUserLocalHome() {
      try {
         InitialContext jndiContext = new InitialContext();

         return (UserLocalHome) jndiContext.lookup("commerce/UserLocalHome"); 
      } catch(Exception e) {
         e.printStackTrace();
         fail("Exception in getUserLocalHome: " + e.getMessage());
      }
      return null;
   }

   public void testDeclaredSql() {
      System.out.println("In testDeclaredSql");

      System.out.println("getUserLocalHome");
      UserLocalHome userLocalHome = getUserLocalHome();

      try {
         System.out.println("creatingUsers");
         UserLocal main = userLocalHome.create("main");
         UserLocal tody1 = userLocalHome.create("tody1");
         UserLocal tody2 = userLocalHome.create("tody2");
         UserLocal tody3 = userLocalHome.create("tody3");
         UserLocal tody4 = userLocalHome.create("tody4");

         Collection userIds = main.getUserIds();

         System.out.println("test it");
         assertTrue(userIds.size() == 5);
         Iterator i = userIds.iterator();
         assertTrue(i.next().equals("main"));
         assertTrue(i.next().equals("tody1"));
         assertTrue(i.next().equals("tody2"));
         assertTrue(i.next().equals("tody3"));
         assertTrue(i.next().equals("tody4"));

         System.out.println("done testDeclaredSql");

      } catch(Exception e) {
         e.printStackTrace();
         fail("Error in testDeclaredSql");
      }
   }
   

   public void setUpEJB() throws Exception {
      System.out.println("delete all users");
      deleteAllUsers(getUserLocalHome());
      System.out.println("done delete all users");
   }
   
   public void tearDownEJB() throws Exception {
   }
   
   public void deleteAllUsers(UserLocalHome userLocalHome) throws Exception {
      Iterator currentUsers = userLocalHome.findAll().iterator();
      while(currentUsers.hasNext()) {
         UserLocal user = (UserLocal)currentUsers.next();
         user.remove();
      }   
   }
}



