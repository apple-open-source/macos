/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import javax.resource.ResourceException;

/**
 * The Interaction enables a component to execute functions on the underlying
 * resource.  An object implementing the Interaction interface supports 
 * two execute() methods for interacting with the underlying resource.
 *
 * An Interaction is created from a Connection and maintains an association
 * with the Connection for its entire lifetime.
 */

public interface Interaction {

    /**
     * Clears all warnings reported by this Interaction.
     * @exception ResourceException Thrown if operation fails.
     */
    public void clearWarnings() throws ResourceException;

    /**
     * Closes an interaction 
     * @exception ResourceException Thrown if operation fails.
     */
    public void close() throws ResourceException;

    /**
     * Executes the interaction specified by the InteractionSpec with 
     * the specified input.
     * @param spec Represents the target function on the underlying resource.
     * @param input Input Record
     * @returns Record Output if successful, null if not.
     * @exception ResourceException Thrown if Interaction fails.
     */
    public Record execute( InteractionSpec spec, Record input )
    	throws ResourceException;

    /**
     * Executes the interaction specified by the InteractionSpec with 
     * the specified input.
     * @param spec Represents the target function on the underlying resource.
     * @param input Input Record
     * @param output Output record
     * @returns boolean True if successful, false if not
     * @exception ResourceException Thrown if Interaction fails.
     */
    public boolean execute( InteractionSpec spec, Record input, Record output )
    	throws ResourceException;

    /**
     * Gets the connection associated with this interaction.
     * @returns Connection Associated connection
     * @exception ResourceException Thrown if operation fails.
     */
    public Connection getConnection() throws ResourceException;

    /**
     * Gets the first warning for this interaction.
     * @returns ResourceWarning First warning.
     * @exception ResourceException Thrown if operation fails.
     */
    public ResourceWarning getWarnings() throws ResourceException;
}
