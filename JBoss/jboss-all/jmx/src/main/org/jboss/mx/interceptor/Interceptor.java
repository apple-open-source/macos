package org.jboss.mx.interceptor;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.8.2 $
 */
public interface Interceptor
{
   public Object invoke(Invocation invocation) throws InvocationException;

   public Interceptor getNext();
   public Interceptor setNext(final Interceptor interceptor);
}
