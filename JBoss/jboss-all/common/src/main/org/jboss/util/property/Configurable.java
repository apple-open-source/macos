/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property;

/**
 * An interface that allows for dynamic configuration of instance objects
 * with properties.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public interface Configurable
{
   /**
    * Configure this object with the given properties.
    *
    * @param props   Properties to configure from.
    */
   void configure(PropertyMap props);
}
