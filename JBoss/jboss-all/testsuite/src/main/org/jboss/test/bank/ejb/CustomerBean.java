/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bank.ejb;

import java.util.*;

import javax.naming.InitialContext;

import org.jboss.test.util.ejb.EntitySupport;
import org.jboss.test.bank.interfaces.*;

/**
 *      
 *   @see <related>
 *   @author $Author: osh $
 *   @version $Revision: 1.4 $
 */
public class CustomerBean
   extends EntitySupport
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   public String id;
   public String name;
   public Collection accounts;
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public String getId()
   {
      return id;
   }
   
   public void setId(String id)
   {
      this.id = id;
   }
   
   public String getName()
   {
      return name;
   }
   
   public void setName(String name)
   {
      this.name = name;
   }
   
   public Collection getAccounts()
   {
		return accounts;
   }
   
   public void addAccount(Account acct)
   {
   		accounts.add(acct);
   }
   
   public void removeAccount(Account acct)
   {
   		accounts.remove(acct);
   }
   
   // EntityHome implementation -------------------------------------
   public CustomerPK ejbCreate(String id, String name) 
   { 
      setId(id);
      setName(name);
      accounts = new ArrayList();
      
      CustomerPK pk = new CustomerPK();
      pk.id = id;
      pk.name = name;

      return pk;
   }
   
   public void ejbPostCreate(String id, String name) 
   { 
   }
}

/*
 *   $Id: CustomerBean.java,v 1.4 2001/01/20 16:32:51 osh Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: CustomerBean.java,v $
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
 *
 *
 *  
 */
