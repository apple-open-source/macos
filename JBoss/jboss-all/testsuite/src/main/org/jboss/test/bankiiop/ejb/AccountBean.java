/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bankiiop.ejb;

import java.rmi.RemoteException;
import javax.ejb.CreateException;

import org.jboss.test.util.ejb.EntitySupport;
import org.jboss.test.bankiiop.interfaces.Customer;

/**
 *      
 *   @see <related>
 *   @author $Author: reverbel $
 *   @version $Revision: 1.1 $
 */
public abstract class AccountBean
   extends EntitySupport
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public void deposit(float amount)
   {
      setBalance(getBalance()+amount);
   }
   
   public void withdraw(float amount)
   {
      setBalance(getBalance()-amount);
   }

   public abstract float getBalance();
   public abstract void setBalance(float balance);
}

/*
 *   $Id: AccountBean.java,v 1.1 2002/03/15 22:36:28 reverbel Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: AccountBean.java,v $
 *   Revision 1.1  2002/03/15 22:36:28  reverbel
 *   Initial version of the bank test for JBoss/IIOP.
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
