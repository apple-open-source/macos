/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.stream;

/**
 * A stream listener
 *
 * @version <tt>$Revision: 1.1.4.1 $</tt>
 * @author  <a href="mailto:Adrian@jboss.org">Adrian Brock</a>
 */
public interface StreamListener
{
   /**
    * Invoked by notifiying streams
    *
    * @param source the stream
    * @param size the number of bytes since the last notification
    */
   void onStreamNotification(Object source, int size);
}
