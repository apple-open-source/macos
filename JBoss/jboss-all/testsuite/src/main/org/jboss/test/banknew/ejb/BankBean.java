/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.banknew.ejb;

import javax.ejb.CreateException;

import org.jboss.test.banknew.interfaces.BankData;
import org.jboss.test.banknew.interfaces.BankPK;
import org.jboss.test.util.ejb.EntitySupport;

/**
 * The Session bean represents a bank.
 *
 * @author Andreas Schaefer
 * @version $Revision: 1.2 $
 *
 * @ejb:bean name="bank/Bank"
 *           display-name="Bank Entity"
 *           type="CMP"
 *           view-type="remote"
 *           jndi-name="ejb/bank/Bank"
 *           schema="Bank"
 *
 * @ejb:interface extends="javax.ejb.EJBObject"
 *
 * @ejb:home extends="javax.ejb.EJBHome"
 *
 * @ejb:pk extends="java.lang.Object"
 *
 * @ejb:data-object extends="java.lang.Object"
 *                  generate="true"
 *
 * @ejb:finder signature="java.util.Collection findAll()"
 *             query="SELECT OBJECT(o) FROM Bank AS o"
 *
 * @ejb:transaction type="Required"
 *
 * @jboss:table-name table-name="New_Bank"
 *
 * @jboss:create-table create="true"
 *
 * @jboss:remove-table remove="true"
 */
public abstract class BankBean
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
   public abstract String getId();
   
   public abstract void setId( String pId );
   
   /**
    * @ejb:persistent-field
    *
    * @jboss:column-name name="Name"
    **/
   public abstract String getName();
   
   public abstract void setName( String pName );
   
   /**
    * @ejb:persistent-field
    *
    * @jboss:column-name name="Address"
    **/
   public abstract String getAddress();
   
   public abstract void setAddress( String pAddress );
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public abstract void setData( BankData pData );
   
   /**
    * @ejb:interface-method view-type="remote"
    **/
   public abstract BankData getData();
   
   // EntityBean implementation -------------------------------------
   
   /**
    * @ejb:create-method view-type="remote"
    **/
   public BankPK ejbCreate( String pName, String pAddress ) 
      throws CreateException
   { 
      setId( "Bank ( " + System.currentTimeMillis() + " )" );
      setName( pName );
      setAddress( pAddress );
      
      return null;
   }
   
   public void ejbPostCreate( String pName, String pAddress ) 
      throws CreateException
   { 
   }
}

/*
 *   $Id: BankBean.java,v 1.2 2002/05/06 00:07:37 danch Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: BankBean.java,v $
 *   Revision 1.2  2002/05/06 00:07:37  danch
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
 */
