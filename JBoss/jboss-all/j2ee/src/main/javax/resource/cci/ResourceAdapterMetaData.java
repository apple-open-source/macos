/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import javax.resource.ResourceException;

/**
 * The ResourceAdaptetMetaData provides information about the resource
 * adapters implementation.
 *
 * The resource adapter does not require an active connection to exist
 * in order for the client to retrieve and use this data.
 */

public interface ResourceAdapterMetaData {

    /**
     * Gets the resource adapter's name.
     * @return Resource adapter name.
     */
    public String getAdapterName() throws ResourceException;

    /**
     * Gets the resource adapter's short description.
     * @return Resource adapter short description.
     */
    public String getAdapterShortDescription() throws ResourceException;

    /**
     * Gets the resource adapter vendor's name.
     * @return Resource adapter vendor name.
     */
    public String getAdapterVendorName() throws ResourceException;

    /**
     * Gets the resource adapter version.
     * @return Resource adapter version.
     */
    public String getAdapterVersion() throws ResourceException;

    /**
     * Gets information on the InteractionSpec types supported by this
     * resource adapter.
     * @return Array of InteractionSpec names supported.
     */
    public String[] getInteractionSpecsSupported() throws ResourceException;

    /**
     * Gets the Connector specification version supported by this adapter.
     * @return Connector specification version.
     */
    public String getSpecVersion() throws ResourceException;

    /**
     * Returns true if the resource adapter Interaction implementation
     * supports the method
     *   boolean execute( InteractionSpec spec, Record input, Record output ),
     * otherwise returns false
     */
    public boolean supportsExecuteWithInputAndOutputRecord()
              throws ResourceException;

    /**
     * Returns true if the resource adapter Interaction implementation
     * supports the method
     *   boolean execute( InteractionSpec spec, Record input ),
     * otherwise returns false
     */
    public boolean supportsExecuteWithInputRecordOnly()
              throws ResourceException;

    /**
     * Returns true if the resource adapter implementation implements the
     * LocalTransaction interface and supports local transaction demarcation.
     */
    public boolean supportsLocalTransactionDemarcation()
              throws ResourceException;
}
