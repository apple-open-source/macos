/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bankiiop.interfaces;

import java.util.*;
import java.rmi.*;
import javax.ejb.*;

/**
 *      
 *   @see <related>
 *   @author $Author: reverbel $
 *   @version $Revision: 1.1 $
 */
public interface AccountHome
   extends EJBHome
{
   // Constants -----------------------------------------------------
   public static final String COMP_NAME = "java:comp/env/ejb/Account";
   public static final String JNDI_NAME = "bank/Account";
    
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public Account create(AccountData data)
      throws RemoteException, CreateException;
   
   public Account findByPrimaryKey(String id)
      throws RemoteException, FinderException;
      
   public Collection findAll()
      throws RemoteException, FinderException;
      
   public Collection findByOwner(Customer owner)
      throws RemoteException, FinderException;
      
   public Collection findLargeAccounts(int balance)
      throws RemoteException, FinderException;
}

/*
 *   $Id: AccountHome.java,v 1.1 2002/03/15 22:36:28 reverbel Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: AccountHome.java,v $
 *   Revision 1.1  2002/03/15 22:36:28  reverbel
 *   Initial version of the bank test for JBoss/IIOP.
 *
 *   Revision 1.3  2001/01/20 16:32:52  osh
 *   More cleanup to avoid verifier warnings.
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
