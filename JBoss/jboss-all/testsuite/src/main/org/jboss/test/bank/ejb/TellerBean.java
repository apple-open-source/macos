/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bank.ejb;

import java.util.*;

import javax.naming.InitialContext;

import org.jboss.test.util.ejb.SessionSupport;
import org.jboss.test.bank.interfaces.*;
import org.jboss.logging.Logger;

import org.apache.log4j.Category;


/**
 *      
 *   @see <related>
 *   @author $Author: starksm $
 *   @version $Revision: 1.7.4.1 $
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
         log.debug("Invocation #"+invocations++);
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
         BankHome bankHome = (BankHome)new InitialContext().lookup(BankHome.COMP_NAME);
         Bank bank = bankHome.create();
         
         AccountHome home = (AccountHome)new InitialContext().lookup(AccountHome.COMP_NAME);
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
            Account acct = (Account)enum.next();
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
         CustomerHome home = (CustomerHome)new InitialContext().lookup(CustomerHome.COMP_NAME);
         Collection customers = home.findAll();
         
         Iterator enum = customers.iterator();
         while(enum.hasNext())
         {
            Customer cust = (Customer)enum.next();
            if (cust.getName().equals(name))
               return cust;
            
         }

         // Create customer
         BankHome bankHome = (BankHome)new InitialContext().lookup(BankHome.COMP_NAME);
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
