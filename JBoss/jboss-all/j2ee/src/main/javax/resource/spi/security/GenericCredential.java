/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi.security;

import javax.resource.spi.SecurityException;

/**
 * The interface GenericCredential defines a method of representing a
 * security credential for a resource which is independent of the security
 * mechanism.  It can be used to wrap any type of underlying credentials,
 * for example it could be used to wrap Kerberos credentials.  This allows
 * the resource adapter to utilize the credentials for sign-on to the EIS.
 */

public interface GenericCredential {
    /**
     * Gets security data from the credential.
     * @return Credential data
     */
    public byte[] getCredentialData() throws SecurityException;

    /**
     * Returns the mechanism type for the credential
     * @return Mechanism Type
     */
    public String getMechType();

    /**
     * Returns the name of the principal associated with the credential
     * @return Principal name
     */
    public String getName();

    /**
     * Tests object for equality
     */
    public boolean equals( Object other );

    /**
     * Generates a hashCode for this object
     */
    public int hashCode();
}
