package org.jboss.web.catalina.security;

import java.io.IOException;
import javax.servlet.ServletException;
import org.apache.catalina.Request;
import org.apache.catalina.Response;
import org.apache.catalina.ValveContext;
import org.apache.catalina.valves.ValveBase;
import org.jboss.security.SecurityAssociation;

/** A Valve that clears the SecurityAssociation information associated with
 * the request thread.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class SecurityAssociationValve extends ValveBase
{
   public void invoke(Request request, Response response, ValveContext context)
      throws IOException, ServletException
   {
      try
      {
         // Perform the request
         context.invokeNext(request, response);
      }
      finally
      {
         // Clear the SecurityAssociation state
         SecurityAssociation.setPrincipal(null);
         SecurityAssociation.setCredential(null);
      }
   }
}
