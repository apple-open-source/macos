/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import javax.resource.ResourceException;

/**
 * A ResultSet represents tabular data returned from the underlying resource
 * by the execution of an interaction.  The cci.ResultSet is based on the
 * JDBC result set.
 */

public interface ResultSet extends Record, java.sql.ResultSet, Cloneable, java.io.Serializable {
}
