package org.jboss.test.web.ejb;

import javax.naming.InitialContext;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import org.jboss.test.web.interfaces.ReferenceTest;
import org.jboss.test.web.interfaces.StatelessSession;
import org.jboss.test.web.interfaces.StatelessSessionHome;
import org.jboss.test.web.interfaces.ReturnData;
import org.jboss.logging.Logger;

/** A stateless SessionBean 

 @author  Scott.Stark@jboss.org
 @version $Revision: 1.6.4.5 $
 */
public class StatelessSessionBean2 implements SessionBean
{
   static Logger log = Logger.getLogger(StatelessSessionBean2.class);

   private SessionContext sessionContext;

   public void ejbCreate() throws CreateException
   {
      log.debug("ejbCreate() called");
   }

   public void ejbActivate()
   {
      log.debug("ejbActivate() called");
   }

   public void ejbPassivate()
   {
      log.debug("ejbPassivate() called");
   }

   public void ejbRemove()
   {
      log.debug("ejbRemove() called");
   }

   public void setSessionContext(SessionContext context)
   {
      sessionContext = context;
   }

   public String echo(String arg)
   {
      log.debug("echo, arg=" + arg);
      return arg;
   }

   public String forward(String echoArg)
   {
      log.debug("forward, echoArg=" + echoArg);
      String echo = null;
      try
      {
         InitialContext ctx = new InitialContext();
         StatelessSessionHome home = (StatelessSessionHome) ctx.lookup("java:comp/env/ejb/Session");
         StatelessSession bean = home.create();
         echo = bean.echo(echoArg);
      }
      catch (Exception e)
      {
         log.debug("failed", e);
         e.fillInStackTrace();
         throw new EJBException(e);
      }
      return echo;
   }

   public void noop(ReferenceTest test, boolean optimized)
   {
      boolean wasSerialized = test.getWasSerialized();
      log.debug("noop, test.wasSerialized=" + wasSerialized + ", optimized=" + optimized);
      if (optimized && wasSerialized == true)
         throw new EJBException("Optimized call had serialized argument");
      if (optimized == false && wasSerialized == false)
         throw new EJBException("NotOptimized call had non serialized argument");
   }

   public ReturnData getData()
   {
      ReturnData data = new ReturnData();
      data.data = "TheReturnData2";
      return data;
   }

   /** A method deployed with no method permissions */
   public void unchecked()
   {
      log.debug("unchecked");
   }

   /** A method deployed with method permissions such that only a run-as
    * assignment will allow access. 
    */
   public void checkRunAs()
   {
      log.debug("checkRunAs");
   }
}
