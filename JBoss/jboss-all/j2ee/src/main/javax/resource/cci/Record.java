/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.cci;

import javax.resource.ResourceException;

/**
 * The Record interface is the base interface for representing input and
 * output for an Interaction.
 *
 * A Record can be extended in several ways:
 *   MappedRecord based on a Map
 *   IndexedRecord based on a List
 *   ResultSet based on a java.sql.ResultSet
 *   Arbitrary JavaBean
 *
 * Both MappedRecord and IndexedRecord support heirarchical structures of
 * Records with Records within Records.
 */

public interface Record extends Cloneable, java.io.Serializable {

    /**
     * Creae a copy of this Record
     */
    public Object clone() throws CloneNotSupportedException;

    /**
     * Compare two Records for equality
     */
    public boolean equals( Object other );
    
    /*
     * Return a hashcode for this Record
     */
    public int hashCode();

    /**
     * Get the name of this Record.
     */
    public String getRecordName();

    /**
     * Set the name of this Record.
     */
    public void setRecordName( String name );

    /**
     * Get the short description of this Record
     */
    public String getRecordShortDescription();

    /**
     * Set the short description of this Record
     */
    public void setRecordShortDescription( String description );
}
