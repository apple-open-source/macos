/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.stress;

import junit.framework.TestSuite;
/**
 * Test failing clients.
 *
 *
 * @author     <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.1 $
 */

public class RunPublishTwo extends ExceptionListenerTest{
   
   public RunPublishTwo(String name) {
      super(name);
   
   }

   public void runPublishTwo() throws Exception{
      runPublish();
      runPublish();
   }
   
   public static junit.framework.Test suite() throws Exception{
      
      TestSuite suite= new TestSuite();
      suite.addTest(new RunPublishTwo("runPublishTwo"));
      
      return suite;
   }
   public static void main(String[] args) {
      
   }
   
} // RunPublishTwo
