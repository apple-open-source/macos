package org.jboss.test.cmp2.perf.ejb;

import javax.ejb.EJBException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.RemoveException;
import javax.ejb.CreateException;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public abstract class CheckBookEntryBean implements EntityBean
{
   public CheckBookEntryBean()
   {
   }

   public void ejbActivate() throws EJBException
   {
   }

   public void ejbLoad() throws EJBException
   {
   }

   public void ejbPassivate() throws EJBException
   {
   }

   public void ejbRemove() throws RemoveException, EJBException
   {
   }

   public void ejbStore() throws EJBException
   {
   }

   public void setEntityContext(EntityContext ctx) throws EJBException
   {
   }

   public void unsetEntityContext() throws EJBException
   {
   }

   public Integer ejbCreate(Integer entryID) throws CreateException
   {
      this.setEntryID(entryID);
      return null;
   }
   public void ejbPostCreate(Integer entryID) throws CreateException
   {
   }

   public abstract Integer getEntryID();
   public abstract void setEntryID(Integer entryID);

   public abstract double getAmount();
   public abstract void setAmount(double amount);

   public abstract String getCategory();
   public abstract void setCategory(String category);

   public abstract long getTimestamp();
   public abstract void setTimestamp(long timestamp);
}
