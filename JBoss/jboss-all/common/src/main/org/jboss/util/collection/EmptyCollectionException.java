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
 * Thrown to indicate that an operation can not be performed on an empty
 * collection.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class EmptyCollectionException
   extends CollectionException
{
   /**
    * Construct a <code>EmptyCollectionException</code> with the specified 
    * detail message.
    *
    * @param msg  Detail message.
    */
   public EmptyCollectionException(String msg) {
      super(msg);
   }

   /**
    * Construct a <code>EmptyCollectionException</code> with no detail.
    */
   public EmptyCollectionException() {
      super();
   }
}
