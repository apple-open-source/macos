/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.interceptor;

public class InvocationException
    extends Exception
{

   private Throwable t = null;

   public InvocationException(Throwable t)
   {
      super();
      this.t = t;
   }
   
   public InvocationException(Throwable t, String msg)
   {
      super(msg);
      this.t = t;
   }

   public Throwable getTargetException()
   {
      return t;
   }

}
