/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.banknew.ejb;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.rmi.RemoteException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.ejb.EJBException;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import javax.ejb.RemoveException;
import javax.ejb.SessionContext;

import org.jboss.test.util.ejb.SessionSupport;
import org.jboss.test.banknew.interfaces.Account;
import org.jboss.test.banknew.interfaces.AccountData;
import org.jboss.test.banknew.interfaces.AccountHome;
import org.jboss.test.banknew.interfaces.AccountPK;
import org.jboss.test.banknew.interfaces.Constants;
import org.jboss.test.banknew.interfaces.Transaction;
import org.jboss.test.banknew.interfaces.TransactionHome;

/**
 * The Session bean represents the account's business interface
 *
 * @author Andreas Schaefer
 * @version $Revision: 1.1 $
 *
 * @ejb:bean name="bank/AccountSession"
 *           display-name="Account Session"
 *           type="Stateless"
 *           view-type="remote"
 *           jndi-name="ejb/bank/AccountSession"
 *
 * @ejb:interface extends="javax.ejb.EJBObject"
 *
 * @ejb:home extends="javax.ejb.EJBHome"
 *
 * @ejb:pk extends="java.lang.Object"
 *
 * @ejb:transaction type="Required"
 *
 * @ejb:ejb-ref ejb-name="bank/Account"
 *
 * @ejb:ejb-ref ejb-name="bank/Transaction"
 */
public class AccountSessionBean
   extends SessionSupport
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public AccountData getAccount( String pAccountId )
      throws FinderException, RemoteException
   {
      return getAccountHome().findByPrimaryKey(
         new AccountPK( pAccountId )
      ).getData();
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public AccountData getAccount( String pCustomerId, int pType )
      throws FinderException, RemoteException
   {
      return getAccountHome().findByCustomerAndType( pCustomerId, pType ).getData();
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public Collection getAccounts( String pCustomerId )
      throws FinderException, RemoteException
   {
      Collection lAccounts = getAccountHome().findByCustomer( pCustomerId );
      Collection lList = new ArrayList( lAccounts.size() );
      Iterator i = lAccounts.iterator();
      while( i.hasNext() ) {
         lList.add( ( (Account) i.next() ).getData() );
      }
      return lList;
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public Collection getTransactions( String pAccountId )
      throws FinderException, RemoteException
   {
      Collection lTransactions = getTransactionHome().findByAccount( pAccountId );
      Iterator i = lTransactions.iterator();
      Collection lList = new ArrayList( lTransactions.size() );
      while( i.hasNext() ) {
         lList.add( ( (Transaction) i.next() ).getData() );
      }
      return lList;
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public AccountData createAccount( String pCustomerId, int pType, float pInitialDeposit )
      throws CreateException, RemoteException
   {
      AccountData lData = new AccountData();
      lData.setCustomerId( pCustomerId );
      lData.setType( pType );
      lData.setBalance( pInitialDeposit );
      Account lAccount = getAccountHome().create( lData );
      AccountData lNew = lAccount.getData();
      getTransactionHome().create(
         lNew.getId(),
         Constants.INITIAL_DEPOSIT,
         pInitialDeposit,
         "Account Creation"
      );
      return lNew;
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public void removeAccount( String pAccountId )
      throws RemoveException, RemoteException
   {
      try {
         Account lAccount = getAccountHome().findByPrimaryKey(
            new AccountPK( pAccountId )
         );
         removeAccount( lAccount );
      }
      catch( FinderException fe ) {
         // When not found then ignore it because account is already removed
      }
      catch( CreateException ce ) {
         throw new EJBException( ce );
      }
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public void removeAccount( String pCustomerId, int pType )
      throws RemoveException, RemoteException
   {
      try {
         Account lAccount = getAccountHome().findByCustomerAndType(
            pCustomerId,
            pType
         );
         removeAccount( lAccount );
      }
      catch( FinderException fe ) {
         // When not found then ignore it because account is already removed
      }
      catch( CreateException ce ) {
         throw new EJBException( ce );
      }
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public void removeTransactions( String pAccountId )
      throws RemoveException, RemoteException
   {
      try {
         Collection lTransactions = getTransactionHome().findByAccount( pAccountId );
         Iterator i = lTransactions.iterator();
         while( i.hasNext() ) {
            Transaction lTransaction = (Transaction) i.next();
            lTransaction.remove();
         }
      }
      catch( FinderException fe ) {
      }
   }
   
   private void removeAccount( Account pAccount )
      throws RemoveException, CreateException, RemoteException
   {
      AccountData lData = pAccount.getData();
      pAccount.remove();
      getTransactionHome().create(
         lData.getId(),
         Constants.FINAL_WITHDRAW,
         lData.getBalance(),
         "Account Closure"
      );
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public void deposit( String pAccountId, float pAmount )
      throws FinderException, RemoteException
   {
      Account lAccount = getAccountHome().findByPrimaryKey(
         new AccountPK( pAccountId )
      );
      AccountData lData = lAccount.getData();
      lData.setBalance( lData.getBalance() + pAmount );
      lAccount.setData( lData );
      try {
         getTransactionHome().create(
            lData.getId(),
            Constants.DEPOSIT,
            pAmount,
            "Account Deposit"
         );
      }
      catch( CreateException ce ) {
         throw new EJBException( ce );
      }
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public void withdraw( String pAccountId, float pAmount )
      throws FinderException, RemoteException
   {
      Account lAccount = getAccountHome().findByPrimaryKey(
         new AccountPK( pAccountId )
      );
      AccountData lData = lAccount.getData();
      lData.setBalance( lData.getBalance() - pAmount );
      lAccount.setData( lData );
      try {
         getTransactionHome().create(
            lData.getId(),
            Constants.WITHDRAW,
            pAmount,
            "Account Withdraw"
         );
      }
      catch( CreateException ce ) {
         throw new EJBException( ce );
      }
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public void transfer( String pFromAccountId, String pToAccountId, float pAmount )
      throws FinderException, RemoteException
   {
      try {
         withdraw( pFromAccountId, pAmount );
         deposit( pToAccountId, pAmount );
      }
      catch( RemoteException re ) {
         re.printStackTrace();
         throw re;
      }
   }
   
   private AccountHome getAccountHome() {
      try {
         return (AccountHome) new InitialContext().lookup( AccountHome.COMP_NAME );
      }
      catch( NamingException ne ) {
         throw new EJBException( ne );
      }
   }
   
   private TransactionHome getTransactionHome() {
      try {
         return (TransactionHome) new InitialContext().lookup( TransactionHome.COMP_NAME );
      }
      catch( NamingException ne ) {
         throw new EJBException( ne );
      }
   }
   
   // SessionBean implementation ------------------------------------
   public void setSessionContext(SessionContext context) 
   {
      super.setSessionContext(context);
   }
}

/*
 *   $Id: AccountSessionBean.java,v 1.1 2002/05/04 01:08:25 schaefera Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: AccountSessionBean.java,v $
 *   Revision 1.1  2002/05/04 01:08:25  schaefera
 *   Added new Stats classes (JMS related) to JSR-77 implemenation and added the
 *   bank-new test application but this does not work right now properly but
 *   it is not added to the default tests so I shouldn't bother someone.
 *
 *   Revision 1.1.2.2  2002/04/29 21:05:17  schaefera
 *   Added new marathon test suite using the new bank application
 *
 *   Revision 1.1.2.1  2002/04/17 05:07:24  schaefera
 *   Redesigned the banknew example therefore to a create separation between
 *   the Entity Bean (CMP) and the Session Beans (Business Logic).
 *   The test cases are redesigned but not finished yet.
 *
 */
