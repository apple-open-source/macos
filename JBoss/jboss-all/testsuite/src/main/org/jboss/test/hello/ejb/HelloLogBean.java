package org.jboss.test.hello.ejb;

import javax.ejb.EJBException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.CreateException;
import org.jboss.logging.Logger;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public abstract class HelloLogBean implements EntityBean
{
   private static Logger log = Logger.getLogger(HelloLogBean.class);

   public HelloLogBean()
   {
   }

   public String ejbCreate(String msg) throws CreateException
   {
      setHelloArg(msg);
      log.info("ejbCreate, msg=" + msg);
      return null;
   }

   public void ejbPostCreate(String msg)
   {
   }

   public abstract String getHelloArg();
   public abstract void setHelloArg(String echoArg);

   public abstract long getStartTime();
   public abstract void setStartTime(long startTime);

   public abstract long getEndTime();
   public abstract void setEndTime(long endTime);

   public long getElapsedTime()
   {
      long start = getStartTime();
      long end = getEndTime();
      return end - start;
   }

   public void setEntityContext(EntityContext ctx) throws EJBException
   {
   }

   public void unsetEntityContext() throws EJBException
   {
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
