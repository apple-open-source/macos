package org.jboss.web.tomcat.security;

import java.io.IOException;
import java.security.Principal;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;

import org.apache.catalina.Request;
import org.apache.catalina.Response;
import org.apache.catalina.ValveContext;
import org.apache.catalina.Wrapper;
import org.apache.catalina.Container;
import org.apache.catalina.Context;
import org.apache.catalina.valves.ValveBase;

import org.jboss.logging.Logger;
import org.jboss.security.SecurityAssociation;
import org.jboss.security.SimplePrincipal;

/** A Valve that clears the SecurityAssociation information associated with
 * the request thread.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.1.1 $
 */
public class SecurityAssociationValve extends ValveBase
{
   private static Logger log = Logger.getLogger(SecurityAssociationValve.class);
   public static ThreadLocal userPrincipal = new ThreadLocal();

   public void invoke(Request request, Response response, ValveContext context)
       throws IOException, ServletException
   {
      try
      {
         String runAs = null;
         try
         {
            // Select the Wrapper to be used for this Request
            Container target = container.map(request, true);
            Wrapper servlet = null;
            if( target instanceof Wrapper )
               servlet = (Wrapper) target;
            else if( target instanceof Context )
               servlet = (Wrapper) target.map(request, true);
            if( servlet != null )
            {
               runAs = servlet.getRunAs();
               if( log.isTraceEnabled() )
                  log.trace(servlet.getName()+", runAs: "+runAs);
               if( runAs != null )
               {
                  SecurityAssociation.pushRunAsRole(new SimplePrincipal(runAs));
               }
            }
            HttpServletRequest httpRequest = (HttpServletRequest) request.getRequest();
            Principal caller = httpRequest.getUserPrincipal();
            userPrincipal.set(caller);
         }
         catch(Throwable e)
         {
            log.debug("Failed to determine servlet", e);
         }
         // Perform the request
         context.invokeNext(request, response);
         if( runAs != null )
            SecurityAssociation.popRunAsRole();
      }
      finally
      {
         SecurityAssociation.setPrincipal(null);
         SecurityAssociation.setCredential(null);
         userPrincipal.set(null);
      }
   }

}
