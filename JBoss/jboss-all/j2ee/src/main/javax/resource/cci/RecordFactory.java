/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import javax.resource.ResourceException;

/**
 * The RecordFactory interface is used for creating MappedRecord and
 * IndexedRecord instances.
 */

public interface RecordFactory {

    /**
     * Creates a MappedRecord with the specified name.  The name is
     * used as a key into the MetaData.
     */
    public MappedRecord createMappedRecord( String recordName )
    	throws ResourceException;

    /**
     * Creates a IndexedRecord with the specified name.  The name is
     * used as a key into the MetaData.
     */
    public IndexedRecord createIndexedRecord( String recordName )
    	throws ResourceException;
}
