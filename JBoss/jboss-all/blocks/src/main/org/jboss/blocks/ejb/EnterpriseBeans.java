/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.blocks.ejb;

import java.util.Collection;
import java.util.Enumeration;
import java.util.Iterator;

import java.rmi.RemoteException;

import javax.ejb.RemoveException;
import javax.ejb.EJBObject;

/**
 * A collection of <em>EJB</em> utility methods.
 *
 * @version <tt>$Revision: 1.1.1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class EnterpriseBeans
{
   /////////////////////////////////////////////////////////////////////////
   //                            Removal Methods                          //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Remove all of the {@link EJBObject}s listed in the given collection.
    *
    * @param c    Collection of {@link EJBObject}s to remove.
    */
   public static void remove(final Collection c)
      throws RemoveException, RemoteException
   {
      remove(c.iterator());
   }

   /**
    * Remove all of the {@link EJBObject}s listed in the given enumeration.
    *
    * @param enum    Enumeration of {@link EJBObject}s to remove.
    */
   public static void remove(final Enumeration enum)
      throws RemoveException, RemoteException
   {
      while (enum.hasMoreElements()) {
         EJBObject obj = (EJBObject)enum.nextElement();
         obj.remove();
      }
   }

   /**
    * Remove all of the {@link EJBObject}s listed in the given iterator.
    *
    * @param iter    Iterator of {@link EJBObject}s to remove.
    */
   public static void remove(final Iterator iter)
      throws RemoveException, RemoteException
   {
      while (iter.hasNext()) {
         EJBObject obj = (EJBObject)iter.next();
         obj.remove();
         iter.remove();
      }
   }
}
 
