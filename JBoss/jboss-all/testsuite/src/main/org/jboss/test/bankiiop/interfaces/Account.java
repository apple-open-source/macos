/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bankiiop.interfaces;

import java.rmi.*;
import javax.ejb.*;

/**
 *      
 *   @see <related>
 *   @author $Author: reverbel $
 *   @version $Revision: 1.1 $
 */
public interface Account
   extends EJBObject
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public void deposit(float amount)
      throws RemoteException;
   
   public void withdraw(float amount)
      throws RemoteException;

   public float getBalance()
      throws RemoteException;
      
   public Customer getOwner()
      throws RemoteException;
      
   public void setData(AccountData data)
      throws RemoteException;
      
   public AccountData getData()
      throws RemoteException;
}

/*
 *   $Id: Account.java,v 1.1 2002/03/15 22:36:28 reverbel Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: Account.java,v $
 *   Revision 1.1  2002/03/15 22:36:28  reverbel
 *   Initial version of the bank test for JBoss/IIOP.
 *
 *   Revision 1.2  2001/01/07 23:14:35  peter
 *   Trying to get JAAS to work within test suite.
 *
 *   Revision 1.1.1.1  2000/06/21 15:52:38  oberg
 *   Initial import of jBoss test. This module contains CTS tests, some simple examples, and small bean suites.
 *
 *
 *  
 */
