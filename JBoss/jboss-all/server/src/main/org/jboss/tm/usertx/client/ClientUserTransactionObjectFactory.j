/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.tm.usertx.client;

import java.util.Hashtable;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.Reference;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.spi.ObjectFactory;

import javax.transaction.UserTransaction;

/**
 *  This is an object factory for producing client
 *  UserTransactions.
 *  usage for standalone clients.
 *      
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.4 $
 */
public class ClientUserTransactionObjectFactory
   implements ObjectFactory
{
   /**
    *  The <code>UserTransaction</code> this factory will return.
    *  This is evaluated lazily in {@link #getUserTransaction()}.
    */
   static private UserTransaction userTransaction = null;

   /**
    *  Get the <code>UserTransaction</code> this factory will return.
    *  This may return a cached value from a previous call.
    */
   static private UserTransaction getUserTransaction()
   {
      if (userTransaction == null) {
         // See if we have a local TM
         try {
            new InitialContext().lookup("java:/TransactionManager");

            // We execute in the server.
            userTransaction = ServerVMClientUserTransaction.getSingleton();
         } catch (NamingException ex) {
            // We execute in a stand-alone client.
            userTransaction = ClientUserTransaction.getSingleton();
         }
      }
      return userTransaction;
   }

   public Object getObjectInstance(Object obj, Name name,
                                   Context nameCtx, Hashtable environment)
      throws Exception
   {
      Reference ref = (Reference)obj;
 
      if (!ref.getClassName().equals(ClientUserTransaction.class.getName()))
         return null;

      return getUserTransaction();
   }
}

