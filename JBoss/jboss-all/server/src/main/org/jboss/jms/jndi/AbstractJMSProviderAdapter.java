/**
 * JBoss, the OpenSource J2EE webOS.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
package org.jboss.jms.jndi;

import javax.naming.Context;
import javax.naming.NamingException;

/**
 * An abstract implementaion of {@link JMSProviderAdapter}.  Sub-classes must
 * provide connection names via instance initialzation and provide an 
 * implementaion of {@link #getInitialContext}.
 *
 * 6/22/01 - hchirino - The queue/topic jndi references are now configed via JMX
 *
 * @version <pre>$Revision: 1.6 $</pre>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author  <a href="mailto:cojonudo14@hotmail.com">Hiram Chirino</a>
 */
public abstract class AbstractJMSProviderAdapter
    implements JMSProviderAdapter, java.io.Serializable
{
    /** The name of the provider. */
    protected String name;

    /** The provider url. */
    protected String providerURL;

    /** The queue factory name to use. */
    protected String queueFactoryRef;

    /** The topic factory name to use. */
    protected String topicFactoryRef;

    /**
     * Set the name of the provider.
     *
     * @param name    The provider name.
     */
    public void setName(final String name) {
        this.name = name;
    }   

    /**
     * Get the name of the provider.
     *
     * @return  The provider name.
     */
    public final String getName() {
        return name;
    }   

    /**
     * Set the URL that will be used to connect to the JNDI provider.
     *
     * @param url  The URL that will be used to connect.
     */
    public void setProviderUrl(final String url) {
        this.providerURL = url;
    }   

    /**
     * Get the URL that is currently being used to connect to the JNDI 
     * provider.
     *
     * @return     The URL that is currently being used.
     */
    public final String getProviderUrl() {
        return providerURL;
    }   

    /**
     * ???
     * 
     * @return  ???
     */
    public String getQueueFactoryRef() {
        return queueFactoryRef;
    }

    /**
     * ???
     * 
     * @return  ???
     */
    public String getTopicFactoryRef() {
        return topicFactoryRef;
    }

    /**
     * ???
     * 
     * @return  ???
     */
    public void setQueueFactoryRef(String newQueueFactoryRef) {
        queueFactoryRef = newQueueFactoryRef;
    }

    /**
     * ???
     * 
     * @return  ???
     */
    public void setTopicFactoryRef(String newTopicFactoryRef) {
        topicFactoryRef = newTopicFactoryRef;
    }
}
