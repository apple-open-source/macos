/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.banknew.interfaces;

import java.rmi.*;
import javax.ejb.*;

/**
 *      
 *   @see <related>
 *   @author $Author: danch $
 *   @version $Revision: 1.3 $
 *
 *   Changes:
 *   <ul>
 *     <li>2002-05-05 danch changes to compile under JDK 1/4</li>
 *   </ul>
 */
public class BankException
   extends Exception
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   Throwable cause;
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   public BankException(String message)
   {
      super(message);
   }
   
   public BankException(String message, Throwable e)
   {
      super(message);
      
      cause = e;
   }
   
   // Public --------------------------------------------------------
   public Throwable getCause() { return cause; }
   
   public String toString() { return super.toString()+", Cause:"+cause; }
}

/*
 *   $Id: BankException.java,v 1.3 2002/05/05 19:02:27 danch Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: BankException.java,v $
 *   Revision 1.3  2002/05/05 19:02:27  danch
 *   changed 'cause' property type to match 1.4 semantics. Still compiles under 1.3
 *
 *   Revision 1.2  2002/05/04 01:08:26  schaefera
 *   Added new Stats classes (JMS related) to JSR-77 implemenation and added the
 *   bank-new test application but this does not work right now properly but
 *   it is not added to the default tests so I shouldn't bother someone.
 *
 *   Revision 1.1.2.2  2002/04/29 21:05:17  schaefera
 *   Added new marathon test suite using the new bank application
 *
 *   Revision 1.1.2.1  2002/04/15 02:32:26  schaefera
 *   Add a new test version of the bank because the old did no use transactions
 *   and the new uses XDoclet 1.1.2 to generate the DDs and other Java classes.
 *   Also a marathon test is added. Please specify the jbosstest.duration for
 *   how long and the test.timeout (which must be longer than the duration) to
 *   run the test with run_tests.xml, tag marathon-test-and-report.
 *
 *   Revision 1.2  2001/01/07 23:14:35  peter
 *   Trying to get JAAS to work within test suite.
 *
 *   Revision 1.1.1.1  2000/06/21 15:52:38  oberg
 *   Initial import of jBoss test. This module contains CTS tests, some simple examples, and small bean suites.
 *
 *
 *  
 */
