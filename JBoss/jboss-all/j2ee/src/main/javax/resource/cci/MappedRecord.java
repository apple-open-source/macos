/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import javax.resource.ResourceException;

/**
 * The MappedRecord interface is used for key-value map based representation
 * of the Record elements.
 */

public interface MappedRecord extends Record, java.util.Map, java.io.Serializable {
}
