/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.resource.adapter.jms;

import java.io.Serializable;

import javax.jms.TopicConnectionFactory;
import javax.jms.QueueConnectionFactory;

/**
 * An aggregate interface for QueueConnectionFactory and
 * TopicConnectionFactory.  Also marks as serializable.
 *
 * <p>Created: Thu Apr 26 17:01:35 2001
 *
 * @author  <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 * @version <pre>$Revision: 1.2 $</pre>
 */
public interface JmsConnectionFactory 
   extends TopicConnectionFactory,
           QueueConnectionFactory,
           Serializable
{
   // empty
}
