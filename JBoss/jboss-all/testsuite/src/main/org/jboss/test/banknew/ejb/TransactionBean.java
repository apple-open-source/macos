/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.banknew.ejb;

import java.util.Date;

import javax.naming.InitialContext;

import org.jboss.test.util.ejb.EntitySupport;
import org.jboss.test.banknew.interfaces.Account;
import org.jboss.test.banknew.interfaces.TransactionData;
import org.jboss.test.banknew.interfaces.TransactionPK;

/**
 * The Entity bean represents a bank transaction
 *
 * @author Andreas Schaefer
 * @version $Revision: 1.2 $
 *
 * @ejb:bean name="bank/Transaction"
 *           display-name="Bank Transaction Entity"
 *           type="CMP"
 *           view-type="remote"
 *           jndi-name="ejb/bank/Transaction"
 *           schema="Transaction"
 *
 * @ejb:interface extends="javax.ejb.EJBObject"
 *
 * @ejb:home extends="javax.ejb.EJBHome"
 *
 * @ejb:pk extends="java.lang.Object"
 *
 * @ejb:transaction type="Required"
 *
 * @ejb:data-object extends="java.lang.Object"
 *
 * @ejb:finder signature="java.util.Collection findAll()"
 *             query="SELECT OBJECT(o) FROM Transaction AS o"
 *
 * @ejb:finder signature="java.util.Collection findByAccount( java.lang.String pAccountId )"
 *             query="SELECT OBJECT(o) FROM Transaction AS o WHERE o.acountId = ?1"
 *
 * @jboss:finder-query name="findByAccount"
 *                     query="Account_Id = {0}"
 *                     order="Date"
 *
 * @jboss:table-name table-name="New_Transaction"
 *
 * @jboss:create-table create="true"
 *
 * @jboss:remove-table remove="true"
 */
public abstract class TransactionBean
   extends EntitySupport
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------
   
   private static int sId = 0;
   
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
    * @jboss:column-name name="Account_Id"
    **/
   abstract public String getAcountId();
   
   abstract public void setAcountId( String pAcountId );
   
   /**
    * @ejb:persistent-field
    *
    * @jboss:column-name name="Type"
    **/
   abstract public int getType();
   
   abstract public void setType( int pType );
   
   /**
    * @ejb:persistent-field
    *
    * @jboss:column-name name="Amount"
    **/
   abstract public float getAmount();
   
   abstract public void setAmount( float pAmount );
   
   /**
    * @ejb:persistent-field
    *
    * @jboss:column-name name="Date"
    **/
   abstract public Date getDate();
   
   abstract public void setDate( Date pDate );
   
   /**
    * @ejb:persistent-field
    *
    * @jboss:column-name name="Description"
    **/
   abstract public String getDescription();
   
   abstract public void setDescription( String pDescription );
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   abstract public TransactionData getData();
   
   abstract public void setData( TransactionData pDate );
   
   // EntityHome implementation -------------------------------------
   
   /**
    * @ejb:create-method view-type="remote"
    **/
   public TransactionPK ejbCreate( String pAccountId, int pType, float pAmount, String pDescription ) {
      setId( "Tx: " + ( sId++ ) );
      setAcountId( pAccountId );
      setType( pType );
      setAmount( pAmount );
      setDescription( pDescription );
      setDate( new Date() );
      
      return null;
   }
   
   public void ejbPostCreate( String pAccountId, int pType, float pAmount, String pDescription ) 
   { 
   }
   
   /**
    * @ejb:create-method view-type="remote"
    **/
   public TransactionPK ejbCreate( TransactionData pData ) {
      setId( "Tx: " + ( sId++ ) );
      pData.setDate( new Date() );
      setData( pData );
      
      return null;
   }
   
   public void ejbPostCreate( TransactionData pData ) 
   { 
   }
}

/*
 *   $Id: TransactionBean.java,v 1.2 2002/05/06 00:07:37 danch Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: TransactionBean.java,v $
 *   Revision 1.2  2002/05/06 00:07:37  danch
 *   Added ejbql query specs, schema names
 *
 *   Revision 1.1  2002/05/04 01:08:25  schaefera
 *   Added new Stats classes (JMS related) to JSR-77 implemenation and added the
 *   bank-new test application but this does not work right now properly but
 *   it is not added to the default tests so I shouldn't bother someone.
 *
 *   Revision 1.1.2.3  2002/04/30 01:21:23  schaefera
 *   Added some fixes to the marathon test and a windows script.
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
 *   Revision 1.4  2001/01/20 16:32:51  osh
 *   More cleanup to avoid verifier warnings.
 *
 *   Revision 1.3  2001/01/07 23:14:34  peter
 *   Trying to get JAAS to work within test suite.
 *
 *   Revision 1.2  2000/09/30 01:00:54  fleury
 *   Updated bank tests to work with new jBoss version
 *
 *   Revision 1.1.1.1  2000/06/21 15:52:37  oberg
 *   Initial import of jBoss test. This module contains CTS tests, some simple examples, and small bean suites.
 */
