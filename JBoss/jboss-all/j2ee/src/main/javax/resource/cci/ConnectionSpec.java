/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import javax.resource.ResourceException;

/**
 * An ConnectionSpec holds connection specific properties for use by a
 * ConnectionFactory in creating a Connection.n Connection in order
 * to execute a function on the underlying resource.
 *
 * The ConnectionSpec interface should be implemented as a JavaBean in
 * order for ease of tool support.
 *
 * A standard set of properties are defined in the specification.  In
 * addition an implementation may implement additional properties.
 */

public interface ConnectionSpec {
}

