package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import java.util.HashSet;
import java.util.Set;
import java.util.Iterator;
import javax.naming.InitialContext;
import javax.ejb.EJBLocalObject;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;
import org.apache.log4j.Category;
import net.sourceforge.junitejb.EJBTestCase;

public class TxTesterTest extends EJBTestCase {
   public static Test suite() {
      TestSuite testSuite = new TestSuite("TxTesterTest");
      testSuite.addTestSuite(TxTesterTest.class);
      return testSuite;
   }   

   public TxTesterTest(String name) {
      super(name);
   }

   private Category log = Category.getInstance(getClass());
   private TxTesterHome txTesterHome;

   /**
    * Looks up all of the home interfaces and creates the initial data. 
    * Looking up objects in JNDI is expensive, so it should be done once 
    * and cached.
    * @throws Exception if a problem occures while finding the home interfaces,
    * or if an problem occures while createing the initial data
    */
   public void setUp() throws Exception {
      InitialContext jndi = new InitialContext();

      txTesterHome = 
            (TxTesterHome) jndi.lookup("commerce/TxTester"); 
   }

   public void testTxTester_none() throws Exception {
      TxTester txTester = null;
      try {
         txTester = txTesterHome.create();
         boolean result = txTester.accessCMRCollectionWithoutTx();

         if (!result)
            fail("Expected accessCMRCollectionWithoutTx to throw an exception");
      } finally {
         if(txTester != null) {
            txTester.remove();
         }
      }
   }
}
