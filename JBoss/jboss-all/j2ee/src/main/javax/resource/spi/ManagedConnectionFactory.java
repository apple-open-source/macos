/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi;

import javax.resource.ResourceException;

/**
 * A ManagedConnectionFactory is a factory for the creation of
 * ManagedConnection objects and ConnectionFactory objects.  It provides
 * methods which can be used to match ManagedConnetions.
 */

public interface ManagedConnectionFactory 
   extends java.io.Serializable
{
    /**
     * Creates a connection factory instance.
     */
    public Object createConnectionFactory() throws ResourceException;

    /**
     * Creates a connection factory instance.
     */
    public Object createConnectionFactory( ConnectionManager cxManager )
    	throws ResourceException;

    /**
     * Creates a new ManagedConnection
     */
    public ManagedConnection createManagedConnection( javax.security.auth.Subject subject, ConnectionRequestInfo cxRequestInfo ) throws ResourceException;

    /**
     * Tests object for equality
     */
    public boolean equals( Object other );

    /**
     * Gets the logwriter for this instance.
     */
    public java.io.PrintWriter getLogWriter() throws ResourceException;

    /**
     * Generates a hashCode for this object
     */
    public int hashCode();

    /**
     * Returns a matching connection from the set.
     */
    public ManagedConnection matchManagedConnections(
                                  java.util.Set connectionSet,
				  javax.security.auth.Subject subject,
                                  ConnectionRequestInfo cxRequestInfo )
		throws ResourceException;

    /**
     * Sets the logwriter for this instance.
     */
    public void setLogWriter( java.io.PrintWriter out ) throws ResourceException;
}
