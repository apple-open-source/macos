/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: ItemService.java,v 1.1.2.1 2003/09/29 23:46:51 starksm Exp $

package org.jboss.test.webservice.store.server;

import org.jboss.test.webservice.store.StoreException;

import javax.ejb.EJBObject;
import javax.ejb.EJBHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import javax.ejb.SessionContext;
import javax.ejb.SessionBean;
import javax.ejb.RemoveException;

import javax.naming.NamingException;
import javax.naming.InitialContext;

import java.rmi.RemoteException;

/**
 * Management session bean to treat items.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * @created 22.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public interface ItemService extends EJBObject
{
   /**
    * @link dependency
    * @associates Item
    * @label manages
    */
   public void delete(Item item) throws StoreException, RemoteException;

   public Item[] findAll() throws StoreException, RemoteException;

   public Item create(String name) throws StoreException, RemoteException;

   /** home of the itemservice */
   public interface Home extends EJBHome
   {
      public ItemService create() throws CreateException, RemoteException;
   }

   /** itemservice server-side implementation */
   public static class Bean implements SessionBean
   {
      transient private SessionContext ctx;

      protected Item.Home lookupItemHome() throws NamingException
      {
         return (Item.Home) new InitialContext().lookup("java:comp/env/store/ItemHome");
      }

      public Item create(String name) throws StoreException
      {
         try
         {
            try
            {
               Item item = lookupItemHome().findByPrimaryKey(name);
               if (item != null)
               {
                  return item;
               }
            }
            catch (FinderException e)
            {
            }

            return lookupItemHome().create(name);
         }
         catch (NamingException e)
         {
            throw new StoreException(e.getMessage());
         }
         catch (CreateException e)
         {
            throw new StoreException(e.getMessage());
         }
      }

      public void delete(Item item) throws StoreException
      {
         try
         {
            item.remove();
         }
         catch (RemoveException e)
         {
            throw new StoreException(e.getMessage());
         }
      }

      public Item[] findAll() throws StoreException
      {
         try
         {
            return (Item[]) lookupItemHome().findAll().toArray(new Item[0]);
         }
         catch (FinderException e)
         {
            throw new StoreException(e.getMessage());
         }
         catch (NamingException e)
         {
            throw new StoreException(e.getMessage());
         }
      }

      public void ejbCreate()
      {
      }

      public void ejbPostCreate()
      {
      }

      public void setSessionContext(SessionContext ctx)
      {
         this.ctx = ctx;
      }

      public void unsetSessionContext()
      {
         this.ctx = null;
      }

      public void ejbActivate()
      {
      }

      public void ejbPassivate()
      {
      }

      public void ejbRemove()
      {
      }

   }


}