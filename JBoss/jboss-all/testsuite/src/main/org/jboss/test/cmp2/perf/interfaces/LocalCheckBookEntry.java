package org.jboss.test.cmp2.perf.interfaces;

import javax.ejb.EJBLocalObject;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface LocalCheckBookEntry extends EJBLocalObject
{
   public Integer getEntryID();
   public void setEntryID(Integer entryID);

   public double getAmount();
   public void setAmount(double amount);

   public String getCategory();
   public void setCategory(String category);

   public long getTimestamp();
   public void setTimestamp(long timestamp);
}
