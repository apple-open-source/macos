/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bankiiop.interfaces;

import java.rmi.*;
import javax.ejb.*;
import javax.naming.*;

/**
 *      
 *   @see <related>
 *   @author $Author: reverbel $
 *   @version $Revision: 1.1 $
 */
public interface Teller
   extends EJBObject
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public void transfer(Account from, Account to, float amount)
      throws RemoteException, BankException;
   
   public Account createAccount(Customer customer, float balance)
      throws RemoteException, BankException;
      
   public Account getAccount(Customer customer, float balance)
      throws RemoteException, BankException;
      
   public Customer getCustomer(String name)
      throws RemoteException, BankException;
      
   public void transferTest(Account from, Account to, float amount, int iter)
      throws java.rmi.RemoteException, BankException;
}

/*
 *   $Id: Teller.java,v 1.1 2002/03/15 22:36:29 reverbel Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: Teller.java,v $
 *   Revision 1.1  2002/03/15 22:36:29  reverbel
 *   Initial version of the bank test for JBoss/IIOP.
 *
 *   Revision 1.2  2001/01/07 23:14:36  peter
 *   Trying to get JAAS to work within test suite.
 *
 *   Revision 1.1.1.1  2000/06/21 15:52:38  oberg
 *   Initial import of jBoss test. This module contains CTS tests, some simple examples, and small bean suites.
 *
 *
 *  
 */
