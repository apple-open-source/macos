/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: BusinessPartnerService.java,v 1.1.2.1 2003/09/29 23:46:51 starksm Exp $

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
 * Management session bean to treat business partners.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * @created 23.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public interface BusinessPartnerService
   extends EJBObject
{
   /**
    * @link dependency
    * @associates BusinessPartner
    * @label manages
    */
   public BusinessPartner create(String name) throws StoreException, RemoteException;

   public void delete(BusinessPartner bp) throws StoreException, RemoteException;

   public BusinessPartner[] findAll() throws StoreException, RemoteException;

   public void update(BusinessPartner bp) throws StoreException, RemoteException;

   public BusinessPartner findByName(String name) throws StoreException, RemoteException;

   /** home of the bpservice */
   public interface Home extends EJBHome
   {
      public BusinessPartnerService create() throws CreateException, RemoteException;
   }

   /** bpservice server-side implementation */
   public static class Bean implements SessionBean
   {
      transient private SessionContext ctx;

      protected BusinessPartner.Home lookupBPHome() throws NamingException
      {
         return (BusinessPartner.Home) new InitialContext().lookup("java:comp/env/store/BusinessPartnerHome");
      }

      public void update(BusinessPartner bp)
      {
      }


      public BusinessPartner findByName(String name) throws StoreException
      {
         try
         {
            return lookupBPHome().findByPrimaryKey(name);
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

      public BusinessPartner create(String name) throws StoreException
      {
         try
         {
            try
            {
               BusinessPartner bp = lookupBPHome().findByPrimaryKey(name);
               if (bp != null)
               {
                  return bp;
               }
            }
            catch (FinderException e)
            {
            }

            return lookupBPHome().create(name);
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

      public void delete(BusinessPartner bp) throws StoreException
      {
         try
         {
            bp.remove();
         }
         catch (RemoveException e)
         {
            throw new StoreException(e.getMessage());
         }
      }

      public BusinessPartner[] findAll() throws StoreException
      {
         try
         {
            return (BusinessPartner[]) lookupBPHome().findAll().toArray(new BusinessPartner[0]);
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

      public void ejbPostCreate(String id, String name)
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