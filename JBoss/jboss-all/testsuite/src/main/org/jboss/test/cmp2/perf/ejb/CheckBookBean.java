package org.jboss.test.cmp2.perf.ejb;

import java.util.Collection;
import javax.ejb.EJBException;
import javax.ejb.EJBLocalHome;
import javax.ejb.EJBLocalObject;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.RemoveException;
import javax.ejb.CreateException;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public abstract class CheckBookBean implements EntityBean
{
   public CheckBookBean()
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

   public void remove()
      throws RemoveException, EJBException
   {
   }

   public String ejbCreate(String account, double balance) throws CreateException
   {
      this.setAccount(account);
      this.setBalance(balance);
      return null;
   }
   public void ejbPostCreate(String account, double balance) throws CreateException
   {

   }

   public abstract Collection getCheckBookEntries();
   public abstract void setCheckBookEntries(Collection entries);

   public abstract String getAccount();
   public abstract void setAccount(String account);
   public abstract double getBalance();
   public abstract void setBalance(double balance);
}
