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
import org.jboss.test.banknew.interfaces.AccountSession;
import org.jboss.test.banknew.interfaces.AccountSessionHome;
// import org.jboss.test.banknew.interfaces.AccountPK;
import org.jboss.test.banknew.interfaces.Constants;
import org.jboss.test.banknew.interfaces.Customer;
import org.jboss.test.banknew.interfaces.CustomerData;
import org.jboss.test.banknew.interfaces.CustomerHome;
import org.jboss.test.banknew.interfaces.CustomerPK;

/**
 * The Session bean represents the customer's business interface
 *
 * @author Andreas Schaefer
 * @version $Revision: 1.1 $
 *
 * @ejb:bean name="bank/CustomerSession"
 *           display-name="Customer Session"
 *           type="Stateless"
 *           view-type="remote"
 *           jndi-name="ejb/bank/CustomerSession"
 *
 * @ejb:interface extends="javax.ejb.EJBObject"
 *
 * @ejb:home extends="javax.ejb.EJBHome"
 *
 * @ejb:pk extends="java.lang.Object"
 *
 * @ejb:transaction type="Required"
 *
 * @ejb:ejb-ref ejb-name="bank/AccountSession"
 *
 * @ejb:ejb-ref ejb-name="bank/Customer"
 */
public class CustomerSessionBean
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
   public CustomerData createCustomer( String pBankId, String pName, float pInitialDeposit )
      throws CreateException, RemoteException
   {
      Customer lCustomer = getCustomerHome().create(
         pBankId,
         pName
      );
      CustomerData lNew = lCustomer.getData();
      getAccountHome().create().createAccount(
         lNew.getId(),
         Constants.CHECKING,
         pInitialDeposit
      );
      return lNew;
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public void removeCustomer( String pCustomerId )
      throws RemoveException, RemoteException
   {
      try {
         getCustomerHome().findByPrimaryKey(
            new CustomerPK( pCustomerId )
         ).remove();
      }
      catch( FinderException fe ) {
         // When not found then ignore it because customer is already removed
      }
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public CustomerData getCustomer( String pCustomerId )
      throws FinderException, RemoteException
   {
      Customer lCustomer = getCustomerHome().findByPrimaryKey(
         new CustomerPK( pCustomerId )
      );
      return lCustomer.getData();
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public Collection getCustomers( String pBankId )
      throws FinderException, RemoteException
   {
      Collection lCustomers = getCustomerHome().findByBank(
         pBankId
      );
      Collection lList = new ArrayList( lCustomers.size() );
      Iterator i = lCustomers.iterator();
      while( i.hasNext() ) {
         lList.add( ( (Customer) i.next() ).getData() );
      }
      return lList;
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public Collection getAccounts( String pCustomerId )
      throws FinderException, RemoteException
   {
      try {
         return getAccountHome().create().getAccounts( pCustomerId );
      }
      catch( CreateException ce ) {
         throw new EJBException( ce );
      }
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public AccountData createAccount( String pCustomerId, int pType, float pInitialDeposit )
      throws CreateException, RemoteException
   {
      return getAccountHome().create().createAccount(
         pCustomerId,
         pType,
         pInitialDeposit
      );
   }
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public void removeAccount( String pCustomerId, int pType )
      throws RemoveException, RemoteException
   {
      try {
         getAccountHome().create().removeAccount( pCustomerId, pType );
      }
      catch( CreateException ce ) {
         // When not found then ignore it because account is already removed
      }
   }
   
   private AccountSessionHome getAccountHome() {
      try {
         return (AccountSessionHome) new InitialContext().lookup( AccountSessionHome.COMP_NAME );
      }
      catch( NamingException ne ) {
         throw new EJBException( ne );
      }
   }
   
   private CustomerHome getCustomerHome() {
      try {
         return (CustomerHome) new InitialContext().lookup( CustomerHome.COMP_NAME );
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
 *   $Id: CustomerSessionBean.java,v 1.1 2002/05/04 01:08:25 schaefera Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: CustomerSessionBean.java,v $
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
 *   Revision 1.1.2.2  2002/04/15 04:28:15  schaefera
 *   Minor fixes regarding to the JNDI names of the beans.
 *
 *   Revision 1.1.2.1  2002/04/15 02:32:24  schaefera
 *   Add a new test version of the bank because the old did no use transactions
 *   and the new uses XDoclet 1.1.2 to generate the DDs and other Java classes.
 *   Also a marathon test is added. Please specify the jbosstest.duration for
 *   how long and the test.timeout (which must be longer than the duration) to
 *   run the test with run_tests.xml, tag marathon-test-and-report.
 *
 *   Revision 1.2  2001/01/07 23:14:34  peter
 *   Trying to get JAAS to work within test suite.
 *
 *   Revision 1.1.1.1  2000/06/21 15:52:37  oberg
 *   Initial import of jBoss test. This module contains CTS tests, some simple examples, and small bean suites.
 *
 *
 *  
 */
