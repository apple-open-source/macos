/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.test.hello.interfaces;

/**
 *  @author Scott.Stark@jboss.org
 *  @version $Revision: 1.1.4.1 $
 */
public class HelloException extends java.lang.Exception
{
   
   /** Creates a new instance of <code>HelloException</code> without detail
    * message.
    */
   public HelloException()
   {
   }

   /** Constructs an instance of <code>HelloException</code> with the specified
    * detail message.
    * @param msg the detail message.
    */
   public HelloException(String msg)
   {
      super(msg);
   }
}
