package org.jboss.test.security.ejb;

import java.io.IOException;
import java.security.Principal;
import javax.ejb.CreateException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import org.apache.log4j.Logger;

/** A simple session bean for testing custom security.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.3.4.1 $
 */
public class IOStatefulSessionBean implements SessionBean
{
   static Logger log = Logger.getLogger(IOStatefulSessionBean.class);

   private SessionContext sessionContext;
   private String sessionPath;

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

   public void setPath(String path)
   {
      this.sessionPath = path;
   }

   public String retryableRead(String path) throws IOException
   {
      return read(path);
   }

   public String read(String path) throws IOException
   {
      log.debug("read, path=" + path);
      Principal p = sessionContext.getCallerPrincipal();
      log.debug("read, callerPrincipal=" + p);
      return path;
   }

   public void write(String path) throws IOException
   {
      log.debug("write, path=" + path);
      Principal p = sessionContext.getCallerPrincipal();
      log.debug("write, callerPrincipal=" + p);
   }
}
