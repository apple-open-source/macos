/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.bankiiop.ejb;

import java.io.ObjectStreamException;
import java.rmi.RemoteException;
import javax.ejb.CreateException;

import org.jboss.test.bankiiop.interfaces.AccountData;
import org.jboss.test.bankiiop.interfaces.Customer;

/**
 *      
 *   @see <related>
 *   @author $Author: reverbel $
 *   @version $Revision: 1.1 $
 */
public class AccountBeanCMP
   extends AccountBean
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   public String id;
   public float balance;
   public Customer owner;
   
   private boolean dirty;
   
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
      dirty = true;
   }
   
   public float getBalance()
   {
      return balance;
   }
   
   public void setBalance(float balance)
   {
      this.balance = balance;
      dirty = true;
   }
   
   public Customer getOwner()
   {
      return owner;
   }
   
   public void setOwner(Customer owner)
   {
      this.owner = owner;
      dirty = true;
   }
   
   public void setData(AccountData data)
   {
      setBalance(data.getBalance());
      setOwner(data.getOwner());
   }
   
   public AccountData getData()
   {
      AccountData data = new AccountData();
      data.setId(id);
      data.setBalance(balance);
      data.setOwner(owner);
      return data;
   }
   
   public boolean isModified()
   {
      return dirty;
   }
   
   // EntityBean implementation -------------------------------------
   public String ejbCreate(AccountData data) 
      throws RemoteException, CreateException
   { 
      setId(data.id);
      setData(data);
      dirty = false;
      return null;
   }
   
   public void ejbPostCreate(AccountData data) 
      throws RemoteException, CreateException
   { 
   }
   
   public void ejbLoad()
      throws RemoteException
   {
      super.ejbLoad();
      dirty = false;
   }
}

/*
 *   $Id: AccountBeanCMP.java,v 1.1 2002/03/15 22:36:28 reverbel Exp $
 *   Currently locked by:$Locker:  $
 *   Revision:
 *   $Log: AccountBeanCMP.java,v $
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
