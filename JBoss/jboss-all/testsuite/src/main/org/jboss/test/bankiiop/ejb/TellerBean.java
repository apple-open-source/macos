/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bankiiop.ejb;

import java.util.*;

import javax.rmi.PortableRemoteObject;
import javax.naming.InitialContext;

import org.jboss.test.util.ejb.SessionSupport;
import org.jboss.test.bankiiop.interfaces.*;

import org.apache.log4j.Category;


/**
 *      
 *   @see <related>
 *   @author $Author: reverbel $
 *   @version $Revision: 1.2 $
 */
public class TellerBean
   extends SessionSupport
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   static int invocations;
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public void transfer(Account from, Account to, float amount)
      throws BankException
   {
      try
      {
         Category.getInstance(TellerBean.class.getName()).info("Invocation #"+invocations++);
         from.withdraw(amount);
         to.deposit(amount);
      } catch (Exception e)
      {
         throw new BankException("Could not transfer "+amount+" from "+from+" to "+to, e);
      }
   }
   
   public Account createAccount(Customer customer, float balance)
      throws BankException
   {
      try
      {
         BankHome bankHome = (BankHome)PortableRemoteObject.narrow(
                               new InitialContext().lookup(BankHome.COMP_NAME),
                               BankHome.class);
         Bank bank = bankHome.create();
         
         AccountHome home = (AccountHome)PortableRemoteObject.narrow(
                            new InitialContext().lookup(AccountHome.COMP_NAME),
                            AccountHome.class);
         AccountData data = new AccountData();
         data.setId(bank.createAccountId(customer));
         data.setBalance(balance);
         data.setOwner(customer);
         Account acct = home.create(data);
		 customer.addAccount(acct);
         
         return acct;
      } catch (Exception e)
      {
         log.debug("failed", e);
         throw new BankException("Could not create account", e);
      }
   }
   
   public Account getAccount(Customer customer, float balance)
      throws BankException
   {
      try
      {
         // Check for existing account
         Collection accounts = customer.getAccounts();
         if (accounts.size() > 0)
         {
            Iterator enum = accounts.iterator();
            Account acct = (Account)PortableRemoteObject.narrow(enum.next(),
								Account.class);

            // Set balance
            acct.withdraw(acct.getBalance()-balance);
            
            return acct;
         } else
         {
            // Create account
            return createAccount(customer, balance);
         }
      } catch (Exception e)
      {
         log.debug("failed", e);
         throw new BankException("Could not get account for "+customer, e);
      }
   }
   
   public Customer getCustomer(String name)
      throws BankException
   {
      try
      {
         // Check for existing customer
         CustomerHome home = (CustomerHome)PortableRemoteObject.narrow(
                           new InitialContext().lookup(CustomerHome.COMP_NAME),
                           CustomerHome.class);
         Collection customers = home.findAll();
         
         Iterator enum = customers.iterator();
         while(enum.hasNext())
         {
            Customer cust = 
	       (Customer)PortableRemoteObject.narrow(enum.next(),
						     Customer.class);
            if (cust.getName().equals(name))
               return cust;
            
         }
         
         // Create customer
         BankHome bankHome = (BankHome)PortableRemoteObject.narrow(
                               new InitialContext().lookup(BankHome.COMP_NAME),
                               BankHome.class);
         Bank bank = bankHome.create();
         
         Customer cust = home.create(bank.createCustomerId(), name);
         log.debug("Customer created");
         return cust;
      } catch (Exception e)
      {
         log.debug("failed", e);
         throw new BankException("Could not get customer for "+name, e);
      }
   }
   
   public void transferTest(Account from, Account to, float amount, int iter)
      throws java.rmi.RemoteException, BankException
   {
      for (int i = 0; i < iter; i++)
      {
         from.withdraw(amount);
         to.deposit(amount);
      }
   }
}
/*
 *   $Id: TellerBean.java,v 1.2 2002/05/27 22:41:49 reverbel Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: TellerBean.java,v $
 *   Revision 1.2  2002/05/27 22:41:49  reverbel
 *   Making the bankiiop test work with the multiple invokers code:
 *     - The test client uses the CosNaming jndi provider.
 *     - Beans use ejb-refs to find each other.
 *     - These refs are properly set up for IIOP (in jboss.xml).
 *
 *   Revision 1.1  2002/03/15 22:36:28  reverbel
 *   Initial version of the bank test for JBoss/IIOP.
 *
 *   Revision 1.7  2002/02/16 11:26:57  user57
 *    o System.err, System.out & printStackTrace() 99.9% gone.
 *
 *   Revision 1.6  2002/02/15 06:15:50  user57
 *    o replaced most System.out usage with Log4j.  should really introduce
 *      some base classes to make this mess more maintainable...
 *
 *   Revision 1.5  2001/08/19 14:45:20  d_jencks
 *   Modified TellerBean to use log4j logging
 *
 *   Revision 1.4  2001/08/02 15:54:17  mnf999
 *   TestBankTest update with number of threads and the output for visual feedback on console
 *
 *
 *   Revision 1.3  2001/01/07 23:14:34  peter
 *   Trying to get JAAS to work within test suite.
 *
 *   Revision 1.2  2000/09/30 01:00:55  fleury
 *   Updated bank tests to work with new jBoss version
 *
 *   Revision 1.1.1.1  2000/06/21 15:52:37  oberg
 *   Initial import of jBoss test. This module contains CTS tests, some simple examples, and small bean suites.
 *
 *
 *  
 */
