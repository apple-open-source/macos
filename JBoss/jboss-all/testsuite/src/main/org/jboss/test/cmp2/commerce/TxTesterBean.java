package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import java.util.Iterator;
import javax.ejb.EJBException;
import javax.ejb.CreateException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;
import org.apache.log4j.Category;

public class TxTesterBean implements SessionBean
{
   private SessionContext ctx;
   private OrderHome orderHome;
   private LineItemHome lineItemHome;

   public void ejbCreate() throws CreateException {
      try {
         InitialContext jndiContext = new InitialContext();

         orderHome = (OrderHome) jndiContext.lookup("commerce/Order"); 
         lineItemHome = (LineItemHome) jndiContext.lookup("commerce/LineItem"); 
      } catch(Exception e) {
         throw new CreateException("Error getting OrderHome and " +
               "LineItemHome: " + e.getMessage());
      }
   }

   public boolean accessCMRCollectionWithoutTx()
   {
      Order o;
      LineItem l1;
      LineItem l2;
 
      // create something to work with
      try {
         o = orderHome.create();
         l1 = lineItemHome.create();
         l2 = lineItemHome.create();
      } catch (CreateException ex) {
         throw new EJBException(ex);
      }

      // this should work
      l1.setOrder(o);


      // this should throw an IllegalStateException
      Collection c = o.getLineItems();
      try {
         c.add(l2);
      } catch (IllegalStateException ex) {
         return true;
      }
      return false;
   }

   public void setSessionContext(SessionContext ctx)
   {
      ctx = ctx;
   }

   public void ejbActivate() { }

   public void ejbPassivate() { }

   public void ejbRemove() { }
}
