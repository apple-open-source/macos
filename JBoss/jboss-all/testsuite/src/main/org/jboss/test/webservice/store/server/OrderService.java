/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: OrderService.java,v 1.1.2.1 2003/09/29 23:46:51 starksm Exp $

package org.jboss.test.webservice.store.server;

import org.jboss.test.webservice.store.StoreException;

import javax.ejb.EJBObject;
import javax.ejb.EJBHome;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import javax.ejb.SessionContext;
import javax.ejb.SessionBean;

import javax.naming.NamingException;
import javax.naming.InitialContext;


/**
 * Management session bean to treat orders.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * @created 25.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public interface OrderService extends EJBObject
{
   /**
    * @link dependency
    * @associates Order
    * @label manages
    */
   public Order[] findAll() throws StoreException;

   public Order[] findByBusinessPartner(BusinessPartner partner) throws StoreException;

   public Order create(String id, BusinessPartner partner) throws StoreException;

   /**
    * @link dependency
    * @associates Line
    * @label manages
    */
   public Line createLine(String id, Order order) throws StoreException;

   public void update(Order order);

   /** home of the itemservice */
   public interface Home extends EJBHome
   {
      public OrderService create() throws CreateException;
   }

   /** orderservice server-side implementation */
   public static class Bean implements SessionBean
   {
      transient private SessionContext ctx;

      protected Order.Home lookupOrderHome() throws NamingException
      {
         return (Order.Home) new InitialContext().lookup("java:comp/env/store/OrderHome");
      }

      protected Line.Home lookupLineHome() throws NamingException
      {
         return (Line.Home) new InitialContext().lookup("java:comp/env/store/LineHome");
      }

      public Order create(String id, BusinessPartner partner) throws StoreException
      {
         try
         {
            try
            {
               Order order = lookupOrderHome().findByPrimaryKey(id);
               if (order != null)
               {
                  if (!partner.equals(order.getBusinessPartner()))
                  {
                     throw new StoreException("Existing order has other partner.");
                  }
               }
               else
               {
                  return order;
               }
            }
            catch (FinderException e)
            {
            }

            return lookupOrderHome().create(id, partner);
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

      public Order[] findAll() throws StoreException
      {
         try
         {
            return (Order[]) lookupOrderHome().findAll().toArray(new Order[0]);
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

      public Order[] findByBusinessPartner(BusinessPartner partner) throws StoreException
      {
         try
         {
            return (Order[]) lookupOrderHome().findByBusinessPartner(partner).toArray(new Order[0]);
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

      public void update(Order order)
      {
         // nothing to do
      }

      public Line createLine(String id, Order order) throws StoreException
      {
         try
         {
            return lookupLineHome().create(id);
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