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
 * Thrown to indicate that an operation can not be performed on a full
 * collection.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class FullCollectionException
   extends CollectionException
{
   /**
    * Construct a <code>FullCollectionException</code> with the specified 
    * detail message.
    *
    * @param msg  Detail message.
    */
   public FullCollectionException(String msg) {
      super(msg);
   }

   /**
    * Construct a <code>FullCollectionException</code> with no detail.
    */
   public FullCollectionException() {
      super();
   }
}
