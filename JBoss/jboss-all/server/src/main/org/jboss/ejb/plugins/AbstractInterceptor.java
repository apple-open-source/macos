/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;


import org.jboss.ejb.Container;
import org.jboss.ejb.Interceptor;
import org.jboss.invocation.Invocation;
import org.jboss.logging.Logger;

/**
 * An abstract base class for container interceptors.
 *
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version $Revision: 1.12.4.1 $
 */
public abstract class AbstractInterceptor
      implements Interceptor
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   /** The next interceptor in the chain. */
   protected Interceptor nextInterceptor;
   /** Logging instance */
   protected Logger log = Logger.getLogger(this.getClass());
   /** The container the interceptor is associated with */
   protected Container container;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

   // Interceptor implementation ------------------------------------

   public void setContainer(Container container)
   {
      this.container = container;
   }

   public Container getContainer()
   {
      return container;
   }

   public void setNext(final Interceptor interceptor)
   {
      // assert interceptor != null
      nextInterceptor = interceptor;
   }

   public Interceptor getNext()
   {
      return nextInterceptor;
   }

   public void create() throws Exception
   {
      // empty
   }

   public void start() throws Exception
   {
      // empty
   }

   public void stop()
   {
      // empty
   }

   public void destroy()
   {
      // empty
   }

   public Object invokeHome(final Invocation mi) throws Exception
   {
      // assert mi != null;
      return getNext().invokeHome(mi);
   }

   public Object invoke(final Invocation mi) throws Exception
   {
      // assert mi != null;
      return getNext().invoke(mi);
   }

   // Protected -----------------------------------------------------
}
