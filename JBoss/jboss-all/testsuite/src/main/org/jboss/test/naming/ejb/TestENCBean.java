package org.jboss.test.naming.ejb;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.apache.log4j.Logger;

/** A bean that does nothing but access resources from the ENC
 to test ENC usage.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.7.4.2 $
 */
public class TestENCBean implements SessionBean
{
   Logger log = Logger.getLogger(getClass());

   private SessionContext sessionContext;

   public void ejbCreate() throws CreateException
   {
   }

// --- Begin SessionBean interface methods
   public void ejbActivate()
   {
   }

   public void ejbPassivate()
   {
   }

   public void ejbRemove()
   {
   }

   public void setSessionContext(SessionContext sessionContext) throws EJBException
   {
      this.sessionContext = sessionContext;
      accessENC();
   }

// --- End SessionBean interface methods

   public long stressENC(long iterations)
   {
      long start = System.currentTimeMillis();
      for(int i = 0; i < iterations; i ++)
         accessENC();
      long end = System.currentTimeMillis();
      return end - start;
   }

   private void accessENC()
   {
      try
      {
         // Obtain the enterprise bean’s environment naming context.
         Context initCtx = new InitialContext();
         Context myEnv = (Context) initCtx.lookup("java:comp/env");
         Boolean hasFullENC = (Boolean) myEnv.lookup("hasFullENC");
         log.debug("ThreadContext CL = " + Thread.currentThread().getContextClassLoader());
         log.debug("hasFullENC = " + hasFullENC);
         if (hasFullENC.equals(Boolean.TRUE))
         {
            // This bean should have the full ENC setup of the ENCBean
            testEnvEntries(initCtx, myEnv);
            testEjbRefs(initCtx, myEnv);
            testJdbcDataSource(initCtx, myEnv);
            testMail(initCtx, myEnv);
            testJMS(initCtx, myEnv);
            testURL(initCtx, myEnv);
            testResourceEnvEntries(initCtx, myEnv);
         }
         else
         {
            // This bean should only have the hasFullENC env entry
            try
            {
               Integer i = (Integer) myEnv.lookup("Ints/i0");
               throw new EJBException("Was able to find java:comp/env/Ints/i0 in bean with hasFullENC = false");
            }
            catch (NamingException e)
            {
               // This is what we expect
            }
         }
      }
      catch (NamingException e)
      {
         log.debug("failed", e);
         throw new EJBException(e.toString(true));
      }
   }

   private void testEnvEntries(Context initCtx, Context myEnv) throws NamingException
   {
      // Basic env values
      Integer i = (Integer) myEnv.lookup("Ints/i0");
      log.debug("Ints/i0 = " + i);
      i = (Integer) initCtx.lookup("java:comp/env/Ints/i1");
      log.debug("Ints/i1 = " + i);
      Float f = (Float) myEnv.lookup("Floats/f0");
      log.debug("Floats/f0 = " + f);
      f = (Float) initCtx.lookup("java:comp/env/Floats/f1");
      log.debug("Floats/f1 = " + f);
      String s = (String) myEnv.lookup("Strings/s0");
      log.debug("Strings/s0 = " + s);
      s = (String) initCtx.lookup("java:comp/env/Strings/s1");
      log.debug("Strings/s1 = " + s);
   }

   private void testEjbRefs(Context initCtx, Context myEnv) throws NamingException
   {
      // EJB References
      Object ejb = myEnv.lookup("ejb/bean0");
      if ((ejb instanceof javax.ejb.EJBHome) == false)
         throw new NamingException("ejb/bean0 is not a javax.ejb.EJBHome");
      log.debug("ejb/bean0 = " + ejb);
      ejb = initCtx.lookup("java:comp/env/ejb/bean1");
      log.debug("ejb/bean1 = " + ejb);
      ejb = initCtx.lookup("java:comp/env/ejb/bean2");
      log.debug("ejb/bean2 = " + ejb);
      //ejb = initCtx.lookup("java:comp/env/ejb/remote-bean");
      ejb = null;
      log.debug("ejb/remote-bean = " + ejb);
   }

   private void testJdbcDataSource(Context initCtx, Context myEnv) throws NamingException
   {
      // JDBC DataSource
      Object obj = myEnv.lookup("jdbc/DefaultDS");
      if ((obj instanceof javax.sql.DataSource) == false)
         throw new NamingException("jdbc/DefaultDS is not a javax.sql.DataSource");
      log.debug("jdbc/DefaultDS = " + obj);
   }

   private void testMail(Context initCtx, Context myEnv) throws NamingException
   {
      // JavaMail Session
      Object obj = myEnv.lookup("mail/DefaultMail");
      if ((obj instanceof javax.mail.Session) == false)
         throw new NamingException("mail/DefaultMail is not a javax.mail.Session");
      log.debug("mail/DefaultMail = " + obj);
   }

   private void testJMS(Context initCtx, Context myEnv) throws NamingException
   {
      // JavaMail Session
      Object obj = myEnv.lookup("jms/QueFactory");
      if ((obj instanceof javax.jms.QueueConnectionFactory) == false)
         throw new NamingException("mail/DefaultMail is not a javax.jms.QueueConnectionFactory");
      log.debug("jms/QueFactory = " + obj);
   }

   private void testURL(Context initCtx, Context myEnv) throws NamingException
   {
      // JavaMail Session
      Object obj = myEnv.lookup("url/JBossHomePage");
      if ((obj instanceof java.net.URL) == false)
         throw new NamingException("url/JBossHomePage is not a java.net.URL");
      log.debug("url/SourceforgeHomePage = " + obj);
      obj = myEnv.lookup("url/SourceforgeHomePage");
      log.debug("url/SourceforgeHomePage = " + obj);
   }

   private void testResourceEnvEntries(Context initCtx, Context myEnv) throws NamingException
   {
      Object obj = myEnv.lookup("res/aQueue");
      if ((obj instanceof javax.jms.Queue) == false)
         throw new NamingException("res/aQueue is not a javax.jms.Queue");
      log.debug("res/aQueue = " + obj);
   }

}
