/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.jms.jndi;

import javax.naming.Context;
import javax.naming.NamingException;
import java.io.Serializable;

/**
 * JMSProviderAdapter.java
 *
 * <p>Created: Wed Nov 29 14:15:07 2000
 * 
 * <p>6/22/01 - hchirino - The queue/topic jndi references are now configed via JMX
 *
 * @author  <a href="mailto:cojonudo14@hotmail.com">Hiram Chirino</a>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version $Revision: 1.5 $
 */
public interface JMSProviderAdapter
   extends Serializable
{
   //
   // jason: this should be redesigned to be non-JNDI specific
   //        and only provide accessors for JMS resources by name.
   //

   /**
    * This must return a context which can be closed.
    */
   Context getInitialContext() throws NamingException;
   void setName(String name);
   String getName();
   void setProviderUrl(String url);
   String getProviderUrl();
   String getQueueFactoryRef();
   String getTopicFactoryRef();
   void setQueueFactoryRef(String newQueueFactoryRef);
   void setTopicFactoryRef(String newTopicFactoryRef);
}
