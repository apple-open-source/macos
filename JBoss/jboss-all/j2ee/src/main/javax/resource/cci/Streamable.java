/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;

/**
 * The Streamable interface allows a resource adapter to interact with a
 * Record as a stream of bytes.
 *
 * The Streamable interface is used by a resource adapter.
 */

public interface Streamable {

    /**
     * Read the Streamable from the specified InputStream.
     */
    public void read( InputStream istream ) throws IOException;

    /**
     * Write the Streamable to the specified OutputStream.
     */
    public void write( OutputStream ostream ) throws IOException;
}
