/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bank.interfaces;

import java.rmi.*;
import javax.ejb.*;

/**
 *      
 *   @see <related>
 *   @author $Author: osh $
 *   @version $Revision: 1.3 $
 */
public interface BankHome
   extends EJBHome
{
   // Constants -----------------------------------------------------
   public static final String COMP_NAME = "java:comp/env/ejb/Bank";
   public static final String JNDI_NAME = "bank/Bank";
    
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public Bank create()
      throws RemoteException, CreateException;
   
}

/*
 *   $Id: BankHome.java,v 1.3 2001/01/20 16:32:52 osh Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: BankHome.java,v $
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
