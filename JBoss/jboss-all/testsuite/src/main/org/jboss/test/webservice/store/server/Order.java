/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: Order.java,v 1.1.2.1 2003/09/29 23:46:51 starksm Exp $

package org.jboss.test.webservice.store.server;

import javax.ejb.EJBObject;
import javax.ejb.EJBHome;
import javax.ejb.EntityBean;
import javax.ejb.CreateException;
import javax.ejb.FinderException;
import javax.ejb.EntityContext;
import java.util.Date;
import java.util.Collection;

/**
 * Compact entity bean that is exposed as a value object
 * via Axis.
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * @created 21.03.2002
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public interface Order extends EJBObject
{
   public String getId();

   public void setId(String id);

   public Date getConfirmationDate();

   public Date getDueDate();

   public void setDueDate();

   /**
    * @link association
    * @associates BusinessPartner
    * @label placedBy
    * @supplierCardinality 1
    * @clientCardinality *
    */
   public BusinessPartner getBusinessPartner();

   public void setBusinessPartner(BusinessPartner partner);

   /**
    * @link aggregationByValue
    * @associates Line
    * @label consistsOf
    * @supplierCardinality *
    * @clientCardinality 1
    */
   public Line[] getLines();

   public void setLines(Line[] lines);

   public interface Home extends EJBHome
   {
      public Order create(String id, BusinessPartner bp) throws CreateException;

      public Order findByPrimaryKey(String id) throws FinderException;

      public Collection findAll() throws FinderException;

      public Collection findByBusinessPartner(BusinessPartner bp) throws FinderException;
   }

   public abstract class Bean implements EntityBean
   {
      transient private EntityContext ctx;

      public String ejbCreate(String id, BusinessPartner bp)
      {
         setId(id);
         setBusinessPartner(bp);
         setConfirmationDate(new Date());
         return null;
      }

      public void ejbPostCreate(String id, BusinessPartner bp)
      {
      }

      public abstract String getId();

      public abstract void setId(String id);

      public abstract BusinessPartner getBusinessPartner();

      public abstract void setBusinessPartner(BusinessPartner bp);

      public abstract Date getConfirmationDate();

      public abstract void setConfirmationDate(Date date);

      public abstract Date getDueDate();

      public abstract void setDueDate(Date date);

      public abstract Line[] getLines();

      public abstract void setLines(Line[] lines);

      public void setEntityContext(EntityContext ctx)
      {
         this.ctx = ctx;
      }

      public void unsetEntityContext()
      {
         this.ctx = null;
      }

      public void ejbActivate()
      {
      }

      public void ejbPassivate()
      {
      }

      public void ejbLoad()
      {
      }

      public void ejbStore()
      {
      }

      public void ejbRemove()
      {
      }
   }
}