package org.jboss.test.cts.ejb;

import javax.ejb.EJBException;
import javax.ejb.SessionContext;
import javax.ejb.SessionBean;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import org.jboss.logging.Logger;

/** A session bean that asserts the count of instances active in the
 * methodA business method do not exceed the maxActiveCount.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class StrictlyPooledSessionBean implements SessionBean
{
   private static Logger log = Logger.getLogger(StatelessSessionBean.class);
   /** The class wide max count of instances allows */
   private static int maxActiveCount = 5;
   /** The class wide count of instances active in business code */
   private static int activeCount;

   private SessionContext ctx;

   private static synchronized int incActiveCount()
   {
      return activeCount ++;
   }
   private static synchronized int decActiveCount()
   {
      return activeCount --;
   }

   public void ejbCreate()
   {
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

   public void setSessionContext(SessionContext ctx)
   {
      this.ctx = ctx;
      try
      {
         InitialContext iniCtx = new InitialContext();
         Integer i = (Integer) iniCtx.lookup("java:comp/env/maxActiveCount");
         maxActiveCount = i.intValue();
      }
      catch(NamingException e)
      {
         // Use default count of 5
      }
   }

   public void methodA()
   {
      int count = incActiveCount();
      log.debug("Begin methodA, activeCount="+count+", ctx="+ctx);
      try
      {
         if( count > maxActiveCount )
         {
            String msg = "IllegalState, activeCount > maxActiveCount, "
                  + count + " > " + maxActiveCount;
            throw new EJBException(msg);
         }
         // Sleep to let the client thread pile up
         Thread.currentThread().sleep(1000);
      }
      catch(InterruptedException e)
      {
      }
      finally
      {
         count = decActiveCount();
         log.debug("End methodA, activeCount="+count+", ctx="+ctx);
      }
   }
}
