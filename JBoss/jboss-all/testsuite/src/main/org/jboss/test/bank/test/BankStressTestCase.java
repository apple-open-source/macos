/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bank.test;

import java.util.*;
import java.lang.reflect.*;
import javax.ejb.*;
import javax.naming.*;
import javax.management.*;
import org.jboss.test.bank.interfaces.*;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;

import org.apache.log4j.Category;

/**
 *      
 *   @see <related>
 *   @author Author: d_jencks among many others
 *   @version $Revision: 1.5 $
 */
public class BankStressTestCase
   extends JBossTestCase
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   int idx = 1;
   int iter;
   Exception exc;
   
   // Static --------------------------------------------------------
	
   // Constructors --------------------------------------------------
	public BankStressTestCase(String name)
	{
		super(name);
	}
   
   // Public --------------------------------------------------------
   public void testTeller()
      throws Exception
   {
      TellerHome home = (TellerHome)new InitialContext().lookup(TellerHome.JNDI_NAME);
      Teller teller = home.create();
      
      BankHome bankHome = (BankHome)new InitialContext().lookup(BankHome.JNDI_NAME);
      Bank bank = bankHome.create();
      
      getLog().debug("Acquire customers");
      Customer marc = teller.getCustomer("Marc");
      Customer rickard = teller.getCustomer("Rickard");
      
      getLog().debug("Acquire accounts");
      Account from = teller.getAccount(marc, 200);
      Account to = teller.getAccount(rickard, 200);
      
      getLog().debug("Show balance");
      getLog().debug(from.getPrimaryKey()+":"+from.getBalance());
      getLog().debug(to.getPrimaryKey()+":"+to.getBalance());

      getLog().debug("Transfer money");
      
      long start = System.currentTimeMillis();
      int iter = 10;
      for (int i = 0; i < iter; i++)
         teller.transfer(from, to, 50);
      long end = System.currentTimeMillis();
      getLog().info("Average call time: "+((end - start) / (iter*6)));
      
      getLog().debug("Show balance");
      AccountHome accountHome = (AccountHome)new InitialContext().lookup(AccountHome.JNDI_NAME);
      Collection accts = accountHome.findAll();
      Iterator enum = accts.iterator();
      while(enum.hasNext())
      {
         Account acct = (Account)enum.next();
         AccountData data = acct.getData();
         getLog().debug(data.getId()+"("+data.getOwner().getName()+"):"+data.getBalance());
         acct.withdraw(data.getBalance()); // Clear
      }
      
      teller.remove();
   }
   
   public void testBank()
      throws Exception
   {
      getLog().debug("Get code");
      BankHome bankHome = (BankHome)new InitialContext().lookup(BankHome.JNDI_NAME);
      
      Bank bank = bankHome.create();
      
      getLog().debug("Bank id="+bank.getId());
      bank.remove();
   }
   
/*   public void testCustomer()
      throws Exception
   {
      getLog().debug("Customer test----------------------------------");
      
      getLog().debug("Create Customer");
      CustomerHome customerHome = (CustomerHome)new InitialContext().lookup("Customer");
      Account from, to;
      try
      {
         from = accountHome.findByPrimaryKey("Marc");
         from.deposit(200);
      } catch (FinderException e)
      {
         from = accountHome.create("Marc", 200);
      }
      
      try
      {
         to = accountHome.findByPrimaryKey("Rickard");
      } catch (FinderException e)
      {
         to = accountHome.create("Rickard", 0);
      }
      
      getLog().debug("Show balance");
      getLog().debug(from.getPrimaryKey()+":"+from.getBalance());
      getLog().debug(to.getPrimaryKey()+":"+to.getBalance());

      getLog().debug("Transfer money");
      TellerHome home = (TellerHome)new InitialContext().lookup("Teller");
      Teller teller = home.create();
      
      long start = System.currentTimeMillis();
      for (int i = 0; i < 100; i++)
         teller.transfer(from, to, 50);
      teller.remove();
      
      getLog().debug("Show balance");
      Iterator enum = accountHome.findAll();
      while(enum.hasNext())
      {
         Account acct = (Account)enum.next();
         getLog().debug(acct.getPrimaryKey()+":"+acct.getBalance());
         acct.withdraw(acct.getBalance()); // Clear
      }
      getLog().debug("Teller test done----------------------------------");
   }
*/

   public void testMultiThread()
      throws Exception
   {
      TellerHome home = (TellerHome)new InitialContext().lookup(TellerHome.JNDI_NAME);
      final Teller teller = home.create();
      
      getLog().debug("Acquire customers");
      Customer marc = teller.getCustomer("Marc");
      Customer rickard = teller.getCustomer("Rickard");
      
      getLog().debug("Acquire accounts");
      final Account from = teller.getAccount(marc, 50);
      final Account to = teller.getAccount(rickard, 0);
      
      final Object lock = new Object();
   
     
      iter = getThreadCount();
      final int iterationCount = getIterationCount();
      getLog().info("Start test. "+getThreadCount()+ " threads, "+getIterationCount()+" iterations");
      long start = System.currentTimeMillis();

      for (int i = 0; i < getThreadCount(); i++)
      {
         Thread.sleep(50);
         new Thread(new Runnable()
         {
            public void run()
            {
               Category log = Category.getInstance(getClass().getName());

               try
               {
                  
                  for (int j = 0; j < iterationCount; j++)
                  {
                     if (exc != null) break;
                     
                     teller.transfer(from,to,50);
                     teller.transfer(from,to,-50);
//                     Thread.currentThread().yield();
//                     logdebug(idx++);
                  }
               } catch (Exception e) 
               {
                  exc = e;
               }
               
               synchronized(lock)
               {
                  iter--;
                  log.info("Only "+iter+" left");
                  lock.notifyAll();
               }
            }
         }).start();
      }
      
      synchronized(lock)
      {
         while(iter>0)
         {
            lock.wait();
         }
      }
      
      if (exc != null) throw exc;
      
      long end = System.currentTimeMillis();
      
      getLog().info("Show balance");
      getLog().info(from.getPrimaryKey()+":"+from.getBalance());
      getLog().info(to.getPrimaryKey()+":"+to.getBalance());
      getLog().info("Time:"+(end-start));
      getLog().info("Avg. time/call(ms):"+((end-start)/(getThreadCount()*getIterationCount()*6)));
   }

   public void testMultiThread2()
      throws Exception
   {
      TellerHome home = (TellerHome)new InitialContext().lookup(TellerHome.JNDI_NAME);
      final Teller teller = home.create();
      
      getLog().debug("Acquire customers");

      final Customer marc = teller.getCustomer("Marc");
      final Customer rickard = teller.getCustomer("Rickard");
      
      final Object lock = new Object();
   
      
      iter = getThreadCount();
      final int iterationCount = getIterationCount();
      getLog().info("Start test. "+getThreadCount()+ " threads, "+getIterationCount()+" iterations");
      long start = System.currentTimeMillis();

      for (int i = 0; i < getThreadCount(); i++)
      {
         Thread.sleep(500); // Wait between each client
         new Thread(new Runnable()
         {
            Category log = Category.getInstance(getClass().getName());

            public void run()
            {
               try
               {
                  
                  Account from = teller.createAccount(marc, 50);
                  Account to = teller.createAccount(rickard, 0);
                  
                  for (int j = 0; j < iterationCount; j++)
                  {
                     if (exc != null) break;
                     
                     teller.transfer(from,to,50);
                     teller.transfer(from,to,-50);
//                     Thread.currentThread().yield();
//                     log.debug(idx++);
                  }
               } catch (Exception e) 
               {
                  exc = e;
               }
               
               synchronized(lock)
               {
                  iter--;
                  log.info("Only "+iter+" left");
                  lock.notifyAll();
               }
            }
         }).start();
      }
      
      synchronized(lock)
      {
         while(iter>0)
         {
            lock.wait();
         }
      }
      
      if (exc != null) throw exc;
      
      long end = System.currentTimeMillis();
      
      getLog().info("Time:"+(end-start));
      getLog().info("Avg. time/call(ms):"+((end-start)/(getThreadCount()*getIterationCount()*6)));
   }
   
   public void testTransaction()
      throws Exception
   {
      TellerHome home = (TellerHome)new InitialContext().lookup(TellerHome.JNDI_NAME);
      Teller teller = home.create();
      
      getLog().debug("Acquire customers");
      Customer marc = teller.getCustomer("Marc");
      getLog().debug("Marc acquired");
      Customer rickard = teller.getCustomer("Rickard");
      getLog().debug("Rickard acquired");
      
      getLog().debug("Acquire accounts");
      Account from = teller.getAccount(marc, 50);
      Account to = teller.getAccount(rickard, 0);
      
      getLog().debug("Show balance");
      getLog().debug(from.getPrimaryKey()+":"+from.getBalance());
      getLog().debug(to.getPrimaryKey()+":"+to.getBalance());

      getLog().debug("Transfer money");
      teller.transfer(from, to, 50);
      getLog().debug("Transfer done");
      
      getLog().debug("Show balance");
      getLog().debug(from.getPrimaryKey()+"("+from.getOwner().getName()+"):"+from.getBalance());
      getLog().debug(to.getPrimaryKey()+"("+to.getOwner().getName()+"):"+to.getBalance());
      
      teller.remove();
      
   }

   public void testTransfer()
      throws Exception
   {
         
      TellerHome home = (TellerHome)new InitialContext().lookup(TellerHome.JNDI_NAME);
      Teller teller = home.create();
      
      getLog().debug("Acquire customers");
      Customer marc = teller.getCustomer("Marc");
      getLog().debug("Marc acquired");
      Customer rickard = teller.getCustomer("Rickard");
      getLog().debug("Rickard acquired");
      
      getLog().debug("Acquire accounts");
      Account from = teller.getAccount(marc, 50*getIterationCount());
      Account to = teller.getAccount(rickard, 0);
      
      getLog().debug("Show balance");
      getLog().debug(from.getPrimaryKey()+":"+from.getBalance());
      getLog().debug(to.getPrimaryKey()+":"+to.getBalance());

      getLog().info("Transfer money");
      long start = System.currentTimeMillis();
      teller.transferTest(from, to, 50, getIterationCount());
      long end = System.currentTimeMillis();
      getLog().info("Transfer done");
      getLog().info("Total time(ms):"+(end-start));
      getLog().info("Avg. time/call(ms):"+((end-start)/(getIterationCount()*2)));
      
      getLog().debug("Show balance");
      getLog().debug(from.getPrimaryKey()+":"+from.getBalance());
      getLog().debug(to.getPrimaryKey()+":"+to.getBalance());
      
      teller.remove();
   }
   
   public void testReadOnly()
      throws Exception
   {
         
      TellerHome home = (TellerHome)new InitialContext().lookup(TellerHome.JNDI_NAME);
      Teller teller = home.create();
      
      getLog().debug("Acquire customers");
      Customer marc = teller.getCustomer("Marc");
      getLog().debug("Marc acquired");
      Customer rickard = teller.getCustomer("Rickard");
      getLog().debug("Rickard acquired");
      
      getLog().debug("Acquire accounts");
      Account from = teller.getAccount(marc, 50*getIterationCount());
      Account to = teller.getAccount(rickard, 0);
      
      getLog().info("Do read calls");
      long start = System.currentTimeMillis();
      for (int i = 0; i < getIterationCount(); i++)
      {
         marc.getName();
         from.getBalance();
         rickard.getName();
         to.getBalance();
      }
      long end = System.currentTimeMillis();
      
      getLog().info("Calls done");
      getLog().info("Total time(ms):"+(end-start));
      getLog().info("Avg. time/call(ms):"+((end-start)/(getIterationCount()*4)));
      
      teller.remove();
   }
   
   public void testPassivation()
      throws Exception
   {
      // Create a bunch of customers, to test passivation
         
      CustomerHome home = (CustomerHome)new InitialContext().lookup(CustomerHome.JNDI_NAME);
      
      getLog().info("Create customers");
      
      for (int i = 0; i < getIterationCount(); i++)
         home.create(i+"", "Smith_"+i);
      getLog().debug("Customers created");
      
   }
   
   public void testFinder()
      throws Exception
   {
      //create some accounts
      testPassivation();
      AccountHome home = (AccountHome)new InitialContext().lookup(AccountHome.JNDI_NAME);
      
      getLog().info("Get large accounts");
      Iterator enum = home.findLargeAccounts(-1).iterator();
      while (enum.hasNext())
      {
         Account acct = (Account)enum.next();
         getLog().debug(acct.getOwner().getName()+":"+acct.getBalance());
      }
   }
	
   protected void setUp()
      throws Exception
   {
      getLog().info("Remove accounts");
      {
         AccountHome home = (AccountHome)new InitialContext().lookup(AccountHome.JNDI_NAME);
         Collection accounts = home.findAll();
         Iterator enum = accounts.iterator();
         while(enum.hasNext())
         {
            Account acct = (Account)enum.next();
            getLog().debug("Removing "+acct.getPrimaryKey());
            acct.remove();
         }
      }
      getLog().info("Remove customers");
      {
         CustomerHome home = (CustomerHome)new InitialContext().lookup(CustomerHome.JNDI_NAME);
         Collection customers = home.findAll();
         Iterator enum = customers.iterator();
         while(enum.hasNext())
         {
            Customer cust = (Customer)enum.next();
            getLog().debug("Removing "+cust.getPrimaryKey());
            cust.remove();
         }
      }
   }


   public static Test suite() throws Exception
   {
      return getDeploySetup(BankStressTestCase.class, "bank.jar");
   }



}
