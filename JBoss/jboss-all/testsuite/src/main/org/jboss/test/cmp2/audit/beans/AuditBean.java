/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.audit.beans;

import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;

/**
 * An entity bean with audit fields behind the scenes.
 *
 * @author    Adrian.Brock@HappeningTimes.com
 * @version   $Revision: 1.1.2.1 $
 */
public abstract class AuditBean
   implements EntityBean
{
   private transient EntityContext ctx;

   public AuditBean()
   {
   }

   public String ejbCreate(String id)
      throws CreateException
   {
      setId(id);
      return null;
   }

   public void ejbPostCreate(String id)
   {
   }

   public abstract String getId();
   public abstract void setId(String id);

   public abstract String getStringValue();
   public abstract void setStringValue(String s);

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
