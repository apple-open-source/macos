/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bank.ejb;

import java.rmi.*;
import javax.naming.*;
import javax.ejb.*;

import org.jboss.test.util.ejb.SessionSupport;
import org.jboss.test.bank.interfaces.*;

/**
 *      
 *   @see <related>
 *   @author $Author: user57 $
 *   @version $Revision: 1.3 $
 */
public class BankBean
   extends SessionSupport
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   static final String ID = "java:comp/env/id";
   String id;
   
   // Static --------------------------------------------------------
   static long nextAccountId = System.currentTimeMillis();
   static long nextCustomerId = System.currentTimeMillis();

   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   public String getId()
   {
      return id;
   }
   
   public String createAccountId(Customer customer)
      throws RemoteException
   {
      return getId()+"."+customer.getName()+"."+(nextAccountId++);
   }
   
   public String createCustomerId()
   {
      return getId()+"."+(nextCustomerId++);
   }
   
   // SessionBean implementation ------------------------------------
   public void setSessionContext(SessionContext context) 
   {
      super.setSessionContext(context);
      
      try
      {
         id = (String)new InitialContext().lookup(ID);
      } catch (Exception e)
      {
         log.debug(e);
         throw new EJBException(e);
      }
   }
}

/*
 *   $Id: BankBean.java,v 1.3 2002/02/15 06:15:50 user57 Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: BankBean.java,v $
 *   Revision 1.3  2002/02/15 06:15:50  user57
 *    o replaced most System.out usage with Log4j.  should really introduce
 *      some base classes to make this mess more maintainable...
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
