/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb;


/**
 * Defines the model for a EnterpriseContext instance pool.
 *
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @version $Revision: 1.12.4.2 $
 */
public interface InstancePool
   extends ContainerPlugin
{
   /**
    * Get an instance without identity.
    *
    * <p>Can be used by finders and create-methods, or stateless beans
    *
    * @return    Context/w instance
    *
    * @throws Exception    RemoteException
    */
   EnterpriseContext get() throws Exception;

   /**
    * Return an anonymous instance after invocation.
    *
    * @param ctx    The context to free.
    */
   void free(EnterpriseContext ctx);

   /**
    * Discard an anonymous instance after invocation.
    * This is called if the instance should not be reused, perhaps due to some
    * exception being thrown from it.
    *
    * @param ctx    The context to discard.
    */
   void discard(EnterpriseContext ctx);

   /**
    * Return the size of the pool.
    *
    * @return the size of the pool.
    */
   int getCurrentSize();

   /**
    * Get the maximum size of the pool.
    *
    * @return the size of the pool.
    */
   public int getMaxSize();

}

