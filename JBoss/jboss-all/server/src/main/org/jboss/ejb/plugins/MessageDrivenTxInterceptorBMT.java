/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ejb.plugins;

import javax.ejb.EJBException;
import org.jboss.invocation.Invocation;

/**
 *   This interceptor handles transactions for message BMT beans.
 *
 *   @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 *   @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 *   @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 *   @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *   @version $Revision: 1.12.2.1 $
 */
public class MessageDrivenTxInterceptorBMT
   extends AbstractTxInterceptorBMT
{
   public Object invokeHome(Invocation mi)
   {
      throw new EJBException("No home methods for message beans.");
   }

   public Object invoke(Invocation mi)
      throws Exception
   {
      return invokeNext(mi);
   }
}
