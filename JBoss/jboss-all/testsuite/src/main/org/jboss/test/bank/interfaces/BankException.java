/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bank.interfaces;

import java.rmi.*;
import javax.ejb.*;

/**
 *      
 *   @see <related>
 *   @author $Author: boissier $
 *   @version $Revision: 1.3 $
 */
public class BankException
   extends Exception
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   Exception cause;
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   public BankException(String message)
   {
      super(message);
   }
   
   public BankException(String message, Exception e)
   {
      super(message);
      
      cause = e;
   }
   
   // Public --------------------------------------------------------
   public Throwable getCause() { return cause; }
   
   public String toString() { return super.toString()+", Cause:"+cause; }
}

/*
 *   $Id: BankException.java,v 1.3 2001/12/04 18:36:47 boissier Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: BankException.java,v $
 *   Revision 1.3  2001/12/04 18:36:47  boissier
 *   * In JDK 1.4, the Throwable interface has a new method:
 *     public Throwable getCause()
 *     This causes a conflict with the file BankException.java.
 *     This patch fixes that.
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
