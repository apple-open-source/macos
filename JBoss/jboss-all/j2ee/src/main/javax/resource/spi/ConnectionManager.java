/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * The ConnectionManager interface provides the hook which allows a resource
 * adapter to pass a connection to the Application Server. The Application
 * Server implements this interface in order to control QoS services to
 * the resource adapter for connection pools.
 */

public interface ConnectionManager extends java.io.Serializable {
    /**
     * Gets called by the resource adapter's connection factory
     */
    public Object allocateConnection( ManagedConnectionFactory mcf,
                                      ConnectionRequestInfo cxRequestInfo )
		throws ResourceException;

}
