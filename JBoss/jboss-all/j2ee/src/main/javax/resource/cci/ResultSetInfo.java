/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import javax.resource.ResourceException;

/**
 * The interface ResultSetInfo provides information on the support for
 * the ResultSet interface by an underlying resource.  A component can
 * retrieve a ResultSetInfo from a Connection instance.
 *
 * A resource only needs to support this interface if it also supports
 * the ResultSet interface.
 */

public interface ResultSetInfo {
    /**
     * Indicates whether or not a visible row delete can be detected.
     */
    public boolean deletesAreDetected(int type) throws ResourceException;
            
    /**
     * Indicates whether or not a visible row insert can be detected.
     */
    public boolean insertsAreDetected(int type) throws ResourceException;

    /**
     * Indicates whether deletes made by others are visible. 
     */
    public boolean othersDeletesAreVisible(int type) throws ResourceException;

    /**
     * Indicates whether inserts made by others are visible. 
     */
    public boolean othersInsertsAreVisible(int type) throws ResourceException;

    /**
     * Indicates whether updates made by others are visible. 
     */
    public boolean othersUpdatesAreVisible(int type) throws ResourceException;

    /**
     * Indicates whether deletes made by self are visible. 
     */
    public boolean ownDeletesAreVisible(int type) throws ResourceException;
            
    /**
     * Indicates whether inserts made by self are visible. 
     */
    public boolean ownInsertsAreVisible(int type) throws ResourceException;
            
    /**
     * Indicates whether updates made by self are visible. 
     */
    public boolean ownUpdatesAreVisible(int type) throws ResourceException;
            
    /**
     * Indicates whether or not an resource adapter supports the specified
     * type of ResultSet. 
     */
    public boolean supportsResultSetType(int type) throws ResourceException;

    /**
     * Indicates whether or not a resource adapter supports the concurrency
     * type in combination with the given ResultSet type.
     */
    public boolean supportsResultTypeConcurrency(int type, int concurrency)
         throws ResourceException;

    /**
     * Indicates whether or not a visible row update can be detected.
     */
    public boolean updatesAreDetected(int type) throws ResourceException;
}
