/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.collection;

/**
 * A generic collection exception.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class CollectionException
   extends RuntimeException
{
   /**
    * Construct a <code>CollectionException</code> with the specified 
    * detail message.
    *
    * @param msg  Detail message.
    */
   public CollectionException(String msg) {
      super(msg);
   }

   /**
    * Construct a <code>CollectionException</code> with no detail.
    */
   public CollectionException() {
      super();
   }
}
