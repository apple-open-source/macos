/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bankiiop.ejb;

import java.util.*;

import org.jboss.test.util.ejb.EntitySupport;
import org.jboss.test.bankiiop.interfaces.*;

/**
 *      
 *   @see <related>
 *   @author $Author: reverbel $
 *   @version $Revision: 1.2 $
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
 *   $Id: CustomerBean.java,v 1.2 2002/05/27 22:41:49 reverbel Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: CustomerBean.java,v $
 *   Revision 1.2  2002/05/27 22:41:49  reverbel
 *   Making the bankiiop test work with the multiple invokers code:
 *     - The test client uses the CosNaming jndi provider.
 *     - Beans use ejb-refs to find each other.
 *     - These refs are properly set up for IIOP (in jboss.xml).
 *
 *   Revision 1.1  2002/03/15 22:36:28  reverbel
 *   Initial version of the bank test for JBoss/IIOP.
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
 *
 *
 *  
 */
