/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.marathon.test;

import java.rmi.RemoteException;
import java.rmi.ServerException;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.Random;

import javax.ejb.CreateException;
import javax.ejb.FinderException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.transaction.TransactionRolledbackException;

import org.jboss.test.banknew.interfaces.AccountData;
import org.jboss.test.banknew.interfaces.AccountSession;
import org.jboss.test.banknew.interfaces.AccountSessionHome;
import org.jboss.test.banknew.interfaces.BankData;
import org.jboss.test.banknew.interfaces.BankSession;
import org.jboss.test.banknew.interfaces.BankSessionHome;
import org.jboss.test.banknew.interfaces.Constants;
import org.jboss.test.banknew.interfaces.CustomerData;
import org.jboss.test.banknew.interfaces.CustomerSession;
import org.jboss.test.banknew.interfaces.CustomerSessionHome;
import org.jboss.test.banknew.interfaces.TellerSession;
import org.jboss.test.banknew.interfaces.TellerSessionHome;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.util.deadlock.ApplicationDeadlockException;
import org.jboss.test.JBossTestCase;

import org.apache.log4j.Category;

/**
 * Marathon test case to test JBoss under moderate utilization for
 * a long time (hours or days) to see if there is a memory leak or
 * other long time exceptions
 *
 * @see <related>
 * @author Author: Andreas Schaefer
 * @version $Revision: 1.3.2.1 $
 */
public class BankMarathonTestCase
   extends JBossTestCase
{
   // Constants -----------------------------------------------------
	
   public static final int DEFAULT_DURATION = Constants.ONE_DAY;    // 1 Day (24 hours)
   
   private static final int INITIAL_NUMBER_OF_CUSTOMERS = 100;     // Create this many customer in setup Bank
   
   // Attributes ----------------------------------------------------
   
   private volatile int mCount;
   private volatile Exception mException;
   private volatile boolean mExit = false;
   
   private Context mContext;
   private BankSession mBankSession = null;
   public int mCustomerCount = INITIAL_NUMBER_OF_CUSTOMERS;
   
   // Static --------------------------------------------------------
   
   public static Test suite() throws Exception {
      return getDeploySetup( BankMarathonTestCase.class, "banknew.jar" );
   }
   
   // Constructors --------------------------------------------------
	public BankMarathonTestCase( String pName ) {
		super( pName );
      log.debug( "create test case with name: " + pName );
	}
   
   // Public --------------------------------------------------------
   
   /**
    * Checks if the environment of test is working
    **/
   public void testEnvironment()
      throws Exception
   {
      // Get all session home interfaces first
      log.debug( "testEnvironment(), start" );
      
      BankSessionHome lBankHome = (BankSessionHome) mContext.lookup( BankSessionHome.JNDI_NAME );
      CustomerSessionHome lCustomerHome = (CustomerSessionHome) mContext.lookup( CustomerSessionHome.JNDI_NAME );
      AccountSessionHome lAccountHome = (AccountSessionHome) mContext.lookup( AccountSessionHome.JNDI_NAME );
      TellerSessionHome lTellerHome = (TellerSessionHome) mContext.lookup( TellerSessionHome.JNDI_NAME );
      
      // Create a session bean by default
      BankSession lBank = lBankHome.create();
      CustomerSession lCustomer = lCustomerHome.create();
      AccountSession lAccount = lAccountHome.create();
      TellerSession lTeller = lTellerHome.create();
      
      // Create a bank the root of everything
      BankData lBankData = lBank.createBank( "Andy's TestBank", "12345 XMass Avenue, New JBoss, GA" );
      
      // Check all the methods on teller inteface because that
      // is what we are here working with primarly
      CustomerData lCustomerData = lTeller.createCustomer( lBankData.getId(), "One", 100 );
      CustomerData lCustomerData2 = lTeller.getCustomer( lCustomerData.getId() );
      Collection lCustomers = lTeller.getCustomers( lBankData.getId() );
      
      AccountData lAccountData = lTeller.createAccount( lCustomerData.getId(), Constants.SAVING, 150 );
      AccountData lAccountData2 = lTeller.getAccount( lCustomerData.getId(), Constants.SAVING );
      AccountData lAccountData3 = lTeller.getAccount( lAccountData.getId() );
      AccountData lAccountData4 = lTeller.getAccount( lCustomerData.getId(), Constants.CHECKING );
      Collection lAccounts = lTeller.getAccounts( lCustomerData.getId() );
      
      lTeller.deposit( lAccountData4.getId(), 75 );
      lTeller.withdraw( lAccountData3.getId(), 63 );
      lTeller.transfer( lAccountData4.getId(), lAccountData3.getId(), 52 );
      
      lTeller.removeAccount( lAccountData4.getId() );
      lTeller.removeAccount( lAccountData.getId() );
      lTeller.removeCustomer( lCustomerData.getId() );
      
      lBank.removeBank( lBankData.getId() );
      log.debug( "testEnvironment() ends" );
   }
   
   /**
    * Marathon test which will:
    * - set up test environment (creating bank, customers and accounts)
    * - create test thread
    *   - create or lookup a customer
    *   - create or look up the accounts
    *   - do (a list of):
    *     - withdraw
    *     - transfer (within customer's accounts or with other customer)
    *     - deposit
    *   - maybe the account is deleted
    *   - maybe the customer is deleted
    *   - maybe the transactions are listed
    *
    * A thread represents a teller (either person (can create and delete
    * customer and accounts or a electronic one) which loops other many
    * business interactions, each loop represents a single business inter-
    * action. Therefore each task will sleep and each business interaction
    * as well. Currently this is complete random but it would be also possible
    * to test crunch times by applying a trend
    **/
   public void testMarathon()
      throws Exception
   {
      log.debug( "testMarathon(), start" );
      // Clean Up Environment
      setUp();
      // Setup the environment
      setupBank();
      
      mCount = getThreadCount();
      mExit = false;
      log.debug( "testMarathon(), start marathon, " + getThreadCount() + " threads" );
      
      long lStart = System.currentTimeMillis();
      
      for( int i = 0; i < getThreadCount(); i++ ) {
         Thread.sleep( 100 );
         log.debug( "testMarathon(), create new thread #: " + i );
         new Thread(
            new RegularTeller( i )
         ).start();
         if( mException != null ) {
            // If exception occurrs during thread creation stop it
            break;
         }
      }
      
      // To end the test when an exception is thrown sleep in 'One Minute'
      // junks to check in between if an exception is thrown. If yes then
      // exit the wait loop and wait for all threads to be ended
      int lMinutes = (int) ( getDuration() / ( Constants.ONE_MINUTE ) ) +
         ( getDuration() % ( Constants.ONE_MINUTE ) == 0 ? 0 : 1 );  // Add one minute if it is split minute
      log.debug( "Test runs for: " + lMinutes + " minutes" );
      // Loop over a minute period sleep time to ensure
      // that when an exception is thrown this threads
      // terminate itself, too.
      for( int i = 0; i < lMinutes; i++ ) {
         log.debug( "Sleep for one minute" );
         Thread.sleep( Constants.ONE_MINUTE );
         log.debug( "------------------------------------------------------------------------" );
         log.debug( "Awake after a sleep for one minute ( " + ( lMinutes - i ) + " minutes left" );
         log.debug( "------------------------------------------------------------------------" );
         if( mException != null || mCount == 0 ) {
            // Exception found then terminate test immediately
            break;
         }
      }
      mExit = true;                   // Switch flag to exit all running threads
      while( mCount > 0 ) {           // Check until all threads exited
         log.debug( "testMarathon(), thread count: " + mCount + ", release lock" );
         Thread.sleep( Constants.ONE_SECOND );        // Wait another second to for all threads to exit
      }
      
      long lEnd = System.currentTimeMillis();
      
      log.info( "testMarathon(), time balance" );
      log.info(
         "testMarathon(), total time test was running: " +
         ( ( lEnd - lStart ) / Constants.ONE_MINUTE ) + " minutes."
      );
      
      log.debug( "testMarathon(), ends" );
      if( mException != null ) {
         // Throw exception if one occurred (the last one occurred)
         throw mException;
      }
   }
	
   protected void setUp()
      throws Exception
   {
      log.debug( "setUp(), start" );
      mContext = new InitialContext();
      
      log.info("Remove accounts and customers");
      BankSessionHome lBankHome = (BankSessionHome) mContext.lookup( BankSessionHome.JNDI_NAME );
      CustomerSessionHome lCustomerHome = (CustomerSessionHome) mContext.lookup( CustomerSessionHome.JNDI_NAME );
      AccountSessionHome lAccountHome = (AccountSessionHome) mContext.lookup( AccountSessionHome.JNDI_NAME );
      BankSession lBankSession = lBankHome.create();
      Collection lBanks = lBankSession.getBanks();
      Iterator i = lBanks.iterator();
      while( i.hasNext() ) {
         BankData lBank = (BankData) i.next();
         // Get all customers
         CustomerSession lCustomerSession = lCustomerHome.create();
         Collection lCustomers = lCustomerSession.getCustomers( lBank.getId() );
         Iterator j = lCustomers.iterator();
         while( j.hasNext() ) {
            CustomerData lCustomer = (CustomerData) j.next();
            // Get all accounts
            AccountSession lAccountSession = lAccountHome.create();
            Collection lAccounts = lAccountSession.getAccounts( lCustomer.getId() );
            Iterator k = lAccounts.iterator();
            while( k.hasNext() ) {
               AccountData lAccount = (AccountData) k.next();
               lAccountSession.removeAccount( lAccount.getId() );
            }
            lCustomerSession.removeCustomer( lCustomer.getId() );
         }
         lBankSession.removeBank( lBank.getId() );
      }
      log.debug( "setUp() ends" );
   }
   
   protected int getDuration() {
      return Integer.getInteger(
         "jbosstest.duration",
         DEFAULT_DURATION
      ).intValue();
   }
   
   public void setupBank()
      throws Exception
   {
      //AS ToDo
      log.debug( "setupBank(), create bank" );
      BankData lBank = getBankSession().createBank( "Andy's TestBank", "12345 XMass Avenue, New JBoss, GA" );
      for( int i = 0; i < INITIAL_NUMBER_OF_CUSTOMERS; i++ ) {
         log.debug( "setupBank(), create customer #: " + i );
         CustomerData lCustomer = getCustomerSession().createCustomer( lBank.getId(), "test", 100 );
      }
   }
   
   private BankSession getBankSession()
      throws CreateException, RemoteException, NamingException
   {
      if( mBankSession == null ) {
         mBankSession = ( (BankSessionHome) mContext.lookup( BankSessionHome.JNDI_NAME ) ).create();
      }
      return mBankSession;
   }
   
   private CustomerSession getCustomerSession()
      throws CreateException, RemoteException, NamingException
   {
      return ( (CustomerSessionHome) mContext.lookup( CustomerSessionHome.JNDI_NAME ) ).create();
   }
   
   private AccountSession getAccountSession()
      throws CreateException, RemoteException, NamingException
   {
      return ( (AccountSessionHome) mContext.lookup( AccountSessionHome.JNDI_NAME ) ).create();
   }
   
   class RegularTeller
      implements Runnable
   {
      private int mId = 0;
      private Category mLog = null;
      
      public RegularTeller( int pId ) {
         mId = pId;
         mLog = Category.getInstance( this.getClass().getName() );
      }
      
      public void run() {
         int mAccountCount = 0;
         Random lRandom = new Random();
         mLog.debug( "run(), id: " + mId + ", exit: " + mExit );
         
         try {
            // Let this thread sleep because the next person must ready first
            // This is also here to avoid that the threads start at the same time
            Thread.sleep( lRandom.nextInt( 2 * Constants.ONE_MINUTE ) );
            // Get bank first
            Collection lBanks = getBankSession().getBanks();
            BankData lBank = (BankData) lBanks.iterator().next();
            mLog.debug( "run(), tread id: " + mId + ", got bank: " + lBank );
            // Loop of business interactions
            while( !mExit && mException == null ) {
               CustomerData lCustomer = null;
               mLog.debug( "run(), tread id: " + mId + ", create or find customer" );
               if( lRandom.nextInt( 100 ) < 10 ) {
                  mLog.debug( "run(), thread id: " + mId + ", create new customer" );
                  lCustomer = getCustomerSession().createCustomer( lBank.getId(), "test", 100 );
                  mCustomerCount++;
                  mLog.debug( "run(), thread id: " + mId + ", new customer: " + lCustomer );
               } else {
                  int i = 0;
                  while( lCustomer == null ) {
                     i++;
                     try {
                        int lCustomerId = lRandom.nextInt( mCustomerCount );
                        mLog.debug( "run(), thread id: " + mId + ", look up customer, id: " + lCustomerId );
                        lCustomer = getBankSession().getCustomer( "" + lCustomerId );
                        mLog.debug( "run(), thread id: " + mId + ", found customer: " + lCustomer );
                     }
                     catch( FinderException fe ) {
                        if( i > 100 ) {
                           throw fe;
                        }
                     }
                  }
               }
               mLog.debug( "run(), tread id: " + mId + ", create or find account" );
               // Get accounts and decide if to create a new account
               List lAccounts = (List) getCustomerSession().getAccounts( lCustomer.getId() );
               AccountData lAccount = null;
               if( lRandom.nextInt( 100 ) < 5 ) {
                  try {
                     lAccount = getCustomerSession().createAccount(
                        lCustomer.getId(), lRandom.nextInt( 3 ), 123
                     );
                     mLog.debug( "run(), thread id: " + mId + ", created account: " + lAccount );
                  }
                  catch( CreateException ce ) {
                  }
               }
               if( lAccount == null ) {
                  lAccount = (AccountData) lAccounts.get( lRandom.nextInt( lAccounts.size() ) );
                  mLog.debug( "run(), thread id: " + mId + ", got account: " + lAccount );
               }
               if( lAccount == null ) {
                  throw new RuntimeException( "Could not find an account" );
               }
               // Do some business methods
               int lLoops = lRandom.nextInt( 10 );
               for( int i = 0; i < lLoops; i++ ) {
                  int lSelection = lRandom.nextInt( 4 );
                  mLog.debug( "run(), thread: " + mId + ", business selection : " + lSelection );
                  switch( lSelection ) {
                     case 0:
                        // Withdraw money when balance is greater than 50
                        if( lAccount.getBalance() > 50 ) {
                           getAccountSession().withdraw( lAccount.getId(), lRandom.nextInt( 50 ) );
                        }
                        break;
                     case 1:
                        if( lAccounts.size() > 1 && lAccount.getBalance() > 50 ) {
                           AccountData lOtherAccount = null;
                           while( true ) {
                              lOtherAccount = (AccountData) lAccounts.get( lRandom.nextInt( lAccounts.size() ) );
                              if( lOtherAccount.getType() != lAccount.getType() ) {
                                 // Found another account type
                                 break;
                              }
                           }
                           while( true ) {
                              try {
                                 getAccountSession().transfer( lAccount.getId(), lOtherAccount.getId(), lRandom.nextInt( 50 ) );
                                 break;
                              }
                              catch( ServerException se ) {
                                 checkServerException( se );
                              }
                           }
                        }
                        break;
                     case 2:
                        if( lAccount.getBalance() > 50 ) {
                           List lCustomers = (List) getBankSession().getCustomers( lBank.getId() );
                           if( lCustomers.size() > 1 ) {
                              CustomerData lOtherCustomer = null;
                              while( true ) {
                                 lOtherCustomer = (CustomerData) lCustomers.get( lRandom.nextInt( lCustomers.size() ) );
                                 if( !lOtherCustomer.getId().equals( lCustomer.getId() ) ) {
                                    break;
                                 }
                              }
                              List lAccounts2 = (List) getAccountSession().getAccounts( lOtherCustomer.getId() );
                              if( lAccounts2.size() > 0 ) {
                                 AccountData lOtherAccount = (AccountData) lAccounts2.get( lRandom.nextInt( lAccounts2.size() ) );
                                 while( true ) {
                                    try {
                                       getAccountSession().transfer( lAccount.getId(), lOtherAccount.getId(), lRandom.nextInt( 50 ) );
                                       break;
                                    }
                                    catch( ServerException se ) {
                                       checkServerException( se );
                                    }
                                 }
                              }
                           }
                        }
                        break;
                     case 3:
                        getAccountSession().deposit( lAccount.getId(), lRandom.nextInt( 100 ) );
                        break;
                  }
                  mLog.debug( "run(), thread: " + mId + ", end business iteration, exit: " + mExit );
                  // Check to see if to exit before sleeping
                  if( mException != null || mExit ) {
                     break;
                  }
                  // Let this thread sleep because a person cannot work at light speed
                  Thread.sleep( lRandom.nextInt( 2 * Constants.ONE_MINUTE ) );
                  // Check to see if to exit before starting another loop of business interactions
                  if( mException != null || mExit ) {
                     break;
                  }
               }
            }
         }
         catch( Exception e ) {
            mLog.error( "run(), got exception", e );
            // Preserve the first exception
            if( mException == null ) {
               mException = e;
               // Terminate all other threads as well
               mExit = true;
            }
         }
         
         mCount--;
         mLog.debug( "run(), thread exists, only " + mCount + " active threads left" );
      }
      
      private void checkServerException( ServerException pException )
         throws ServerException
      {
         Throwable lThrowable = pException.detail;
         if( lThrowable instanceof ApplicationDeadlockException ) {
            mLog.debug( "Found ADE in ServerException: " + pException );
            return;
         } else
         if( lThrowable instanceof TransactionRolledbackException ) {
            TransactionRolledbackException lTRE = (TransactionRolledbackException) lThrowable;
            if( lTRE.detail instanceof ApplicationDeadlockException ) {
               mLog.debug( "Found ADE in TransactionRolledbackException: " + lTRE );
               return;
            } else
            if( lTRE.detail instanceof TransactionRolledbackException ) {
               TransactionRolledbackException lTRE2 = (TransactionRolledbackException) lTRE.detail;
               if( lTRE2.detail instanceof ApplicationDeadlockException ) {
                  mLog.debug( "Found ADE in 2. TransactionRolledbackException: " + lTRE2 );
                  return;
               }
            }
         }
         throw pException;
      }
      
   }
}
