/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import java.io.PrintWriter;
import java.io.Serializable;

import javax.resource.Referenceable;
import javax.resource.ResourceException;

/**
 * The ConnectionFactory provides an interface for getting a Connection
 * from the Resource adapter.
 *
 * The application retrieves a reference to the ConnectionFactory through
 * a JNDI lookup.
 *
 * ConnectionFactory extends java.io.Serializable and
 * javax.resource.Referenceable in order to support JNDI lookup.
 */

public interface ConnectionFactory extends Serializable, Referenceable {

    /**
     * Gets a connection from the resource adapter.  When using this
     * method the client does not pass any security information, and
     * wants the container to manage sign-on.  This is called container
     * managed sign-on.
     */
    public Connection getConnection() throws ResourceException;

    /**
     * Gets a connection from the resource adapter.  When using this
     * method the client passes in the security information.  This is
     * called component managed sign-on.
     */
    public Connection getConnection( ConnectionSpec properties )
                throws ResourceException;

    /**
     * Gets a RecordFactory instance for use in creating Record objects.
     */
    public RecordFactory getRecordFactory() throws ResourceException;

    /**
     * Gets metadata for the resource adapter.  This call does not require
     * an active connection.
     */
    public ResourceAdapterMetaData getMetaData() throws ResourceException;
}
