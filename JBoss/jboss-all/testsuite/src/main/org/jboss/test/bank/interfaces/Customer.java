/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bank.interfaces;

import java.rmi.*;
import java.util.*;
import javax.ejb.*;

/**
 *      
 *   @see <related>
 *   @author $Author: peter $
 *   @version $Revision: 1.3 $
 */
public interface Customer
   extends EJBObject
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public String getName()
      throws RemoteException;
      
   public void setName(String name)
      throws RemoteException;
      
   public Collection getAccounts()
      throws RemoteException;
	  
   public void addAccount(Account acct)
		throws RemoteException;
   
   public void removeAccount(Account acct)
		throws RemoteException;
}

/*
 *   $Id: Customer.java,v 1.3 2001/01/07 23:14:36 peter Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: Customer.java,v $
 *   Revision 1.3  2001/01/07 23:14:36  peter
 *   Trying to get JAAS to work within test suite.
 *
 *   Revision 1.2  2000/09/30 01:00:57  fleury
 *   Updated bank tests to work with new jBoss version
 *
 *   Revision 1.1.1.1  2000/06/21 15:52:38  oberg
 *   Initial import of jBoss test. This module contains CTS tests, some simple examples, and small bean suites.
 *
 *
 *  
 */
