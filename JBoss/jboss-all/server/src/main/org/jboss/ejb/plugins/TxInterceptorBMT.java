/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ejb.plugins;

import java.util.Map;

import org.jboss.invocation.Invocation;

import org.jboss.metadata.SessionMetaData;

/**
 *   This interceptor handles transactions for session BMT beans.
 *
 *   @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 *   @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 *   @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 *   @author <a href="mailto:akkerman@cs.nyu.edu">Anatoly Akkerman</a>
 *   @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *   @version $Revision: 1.22 $
 */
public class TxInterceptorBMT
   extends AbstractTxInterceptorBMT
{

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

   // Interceptor implementation --------------------------------------

   public void create()
      throws Exception
   {
      // Do initialization in superclass.
      super.create();
 
      // Set the atateless attribute
      stateless = ((SessionMetaData)container.getBeanMetaData()).isStateless();
   }

   public Object invokeHome(Invocation mi)
      throws Exception
   {
      if (stateless)
         // stateless: no context, no transaction, no call to the instance
         return getNext().invokeHome(mi);
      else
         return invokeNext(mi);
   }

   public Object invoke(Invocation mi)
      throws Exception
   {
      return invokeNext(mi);
   }

  // Monitorable implementation ------------------------------------
  public void sample(Object s)
  {
    // Just here to because Monitorable request it but will be removed soon
  }
  public Map retrieveStatistic()
  {
    return null;
  }
  public void resetStatistic()
  {
  }

   // Protected  ----------------------------------------------------

   // Inner classes -------------------------------------------------

}

