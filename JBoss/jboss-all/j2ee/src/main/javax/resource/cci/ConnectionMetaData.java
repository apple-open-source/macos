/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import javax.resource.ResourceException;

/**
 * The ConnectionMetaData provides information about the underlying
 * resources for the connection.
 */

public interface ConnectionMetaData {

    /**
     * Gets the product name of the underlying resource for the connection.
     * @return Product name of underlying resource.
     */
    public String getEISProductName() throws ResourceException;


    /**
     * Gets the product version of the underlying resource for the connection.
     * @return Product version name of underlying resource.
     */
    public String getEISProductVersion() throws ResourceException;


    /**
     * Gets the user name for the connection to the underlying resource
     * as known to the underlying resource.  This name corresponds to
     * the principal under whose context the connection was first made.
     * @return Product name of underlying resource.
     */
    public String getUserName() throws ResourceException;
}
