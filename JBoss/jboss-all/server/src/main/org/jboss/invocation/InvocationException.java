/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.invocation;


/** A nested exception that is used to differentiate application exceptions
 * from communication exceptions.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
*/
public class InvocationException
    extends Exception
{
   private Throwable cause = null;

   public InvocationException(Throwable cause)
   {
      super();
      this.cause = cause;
   }
   
   public InvocationException(String msg, Throwable cause)
   {
      super(msg);
      this.cause = cause;
   }

   public Throwable getTargetException()
   {
      return cause;
   }

}
