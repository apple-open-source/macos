/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import java.io.Serializable;
import javax.resource.ResourceException;

/**
 * An InteractionSpec holds properties for use by an Interaction in order
 * to execute a function on the underlying resource.
 *
 * There is a set of standard properties which are used to give hints
 * to an Interaction object about the requirements of a ResultSet.
 *
 * FetchSize, FetchDirection, MaxFieldSize, ResultSetType, ResultSetConcurrency
 *
 * A specific implementation may implement additional properties.
 */

public interface InteractionSpec extends Serializable {
    /**
     * Execution requires only a send to the underlying resource.
     */
    public static final int SYNC_SEND = 0;

    /**
     * Execution requires only a send to the underlying resource.
     */
    public static final int SYNC_SEND_RECEIVE = 1;

    /**
     * Execution results in a synchronous receive of the output Record
     */
    public static final int SYNC_RECEIVE = 2;
}
