/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.banknew.ejb;

import java.io.ObjectStreamException;
import java.rmi.RemoteException;
import javax.ejb.CreateException;

import org.jboss.test.util.ejb.EntitySupport;
import org.jboss.test.banknew.interfaces.AccountData;
import org.jboss.test.banknew.interfaces.AccountPK;

/**
 * The Entity bean represents a bank account
 *
 * @author Andreas Schaefer
 * @version $Revision: 1.2 $
 *
 * @ejb:bean name="bank/Account"
 *           display-name="Bank Account Entity"
 *           type="CMP"
 *           view-type="remote"
 *           jndi-name="ejb/bank/Account"
 *           schema="Account"
 *
 * @ejb:interface extends="javax.ejb.EJBObject"
 *
 * @ejb:home extends="javax.ejb.EJBHome"
 *
 * @ejb:pk extends="java.lang.Object"
 *
 * @ejb:transaction type="Required"
 *
 * @ejb:data-object setdata="true"
 *                  extends="java.lang.Object"
 *
 * @ejb:finder signature="java.util.Collection findAll()"
 *             query="SELECT OBJECT(o) from Account AS o"
 *
 * @ejb:finder signature="java.util.Collection findByCustomer( java.lang.String pCustomerId )"
 *             query="SELECT OBJECT(o) from Account AS o WHERE o.customerId = ?1"
 *
 * @jboss:finder-query name="findByCustomer"
 *                     query="Customer_Id = {0}"
 *                     order="Type"
 *
 * @ejb:finder signature="org.jboss.test.banknew.interfaces.Account findByCustomerAndType( java.lang.String pCustomerId, int pType )"
 *             query="SELECT OBJECT(o) from Account AS o WHERE o.customerId = ?1 AND o.type = ?2"
 *
 * @jboss:finder-query name="findByCustomerAndType"
 *                     query="Customer_Id = {0} AND Type = {1}"
 *
 * @jboss:table-name table-name="New_Account"
 *
 * @jboss:create-table create="true"
 *
 * @jboss:remove-table remove="true"
 */
public abstract class AccountBean
   extends EntitySupport
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   
   /**
    * @ejb:persistent-field
    * @ejb:pk-field
    *
    * @jboss:column-name name="Id"
    **/
   abstract public String getId();
   
   abstract public void setId( String pId );
   
   /**
    * @ejb:persistent-field
    *
    * @jboss:column-name name="Customer_Id"
    **/
   abstract public String getCustomerId();
   
   abstract public void setCustomerId( String pCustomerId );
   
   /**
    * @ejb:persistent-field
    *
    * @jboss:column-name name="Type"
    **/
   abstract public int getType();
   
   abstract public void setType( int pType );
   
   /**
    * @ejb:persistent-field
    * @ejb:interface-method view-type="remote"
    *
    * @jboss:column-name name="Balance"
    **/
   abstract public float getBalance();
   
   abstract public void setBalance( float pAmount );
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public abstract void setData( AccountData pData );
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public abstract AccountData getData();
   
//   protected abstract void makeDirty();
   
//   protected abstract void makeClean();
   
   // EntityBean implementation -------------------------------------
   /**
    * @ejb:create-method view-type="remote"
    **/
   public AccountPK ejbCreate( AccountData pData )
      throws CreateException
   {
      setId( pData.getCustomerId() + ":" + pData.getType() );
      setData( pData );
      
      return null;
   }
   
   public void ejbPostCreate( AccountData data ) 
      throws CreateException
   { 
   }
}

/*
 *   $Id: AccountBean.java,v 1.2 2002/05/06 00:07:36 danch Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: AccountBean.java,v $
 *   Revision 1.2  2002/05/06 00:07:36  danch
 *   Added ejbql query specs, schema names
 *
 *   Revision 1.1  2002/05/04 01:08:25  schaefera
 *   Added new Stats classes (JMS related) to JSR-77 implemenation and added the
 *   bank-new test application but this does not work right now properly but
 *   it is not added to the default tests so I shouldn't bother someone.
 *
 *   Revision 1.1.2.5  2002/04/30 01:21:23  schaefera
 *   Added some fixes to the marathon test and a windows script.
 *
 *   Revision 1.1.2.4  2002/04/29 21:05:17  schaefera
 *   Added new marathon test suite using the new bank application
 *
 *   Revision 1.1.2.3  2002/04/17 05:07:24  schaefera
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
 *   Revision 1.2.2.1  2001/12/27 22:00:02  starksm
 *
 *   Set dirty to false in ejbStore
 *
 *   Revision 1.2  2001/01/07 23:14:34  peter
 *   Trying to get JAAS to work within test suite.
 *
 *   Revision 1.1.1.1  2000/06/21 15:52:37  oberg
 *   Initial import of jBoss test. This module contains CTS tests, some simple examples, and small bean suites.
 */
