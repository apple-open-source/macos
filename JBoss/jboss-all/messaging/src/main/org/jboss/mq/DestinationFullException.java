/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.mq;

/**
 * Thrown when a message cannot be added to a queue or a subscription
 * because it is full.
 *
 * @author  <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version <tt>$Revision: 1.1.2.1 $</tt>
 */
public class DestinationFullException
   extends SpyJMSException
{
   /** The text of the message before the nested mangles it */
   protected String text;

   public DestinationFullException(final String msg)
   {
      super(msg);
      this.text = msg;
   }

   public String getText()
   {
      return text;
   }
}
