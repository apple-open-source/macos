/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.management.j2ee;

/**
* Exception containing another target exception
* for further informations
*
* @author <a href="mailto:andreas@jboss.com">Andreas Schaefer</a>
* @version $Revision: 1.2 $
**/
public abstract class WrapperException
   extends Exception
{

   public Throwable mTarget;
   
   public WrapperException( String pMessage, Throwable pThrowable ) {
      super( pMessage );
      mTarget = pThrowable;
   }
   
   public Throwable getTargetException() {
      return mTarget;
   }
}
