/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.resource.adapter.jms;

import javax.jms.TopicConnection;
import javax.jms.QueueConnection;

/**
 * A marker interface to join topics and queues into one factory.
 *
 * <p>Created: Thu Mar 29 15:37:21 2001
 *
 * @author  <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 * @version <pre>$Revision: 1.1 $</pre>
 */
public interface JmsSessionFactory
   extends TopicConnection, QueueConnection
{
   // empty
}
