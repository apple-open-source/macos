/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource.spi.security;

import javax.resource.spi.ManagedConnectionFactory;
import javax.resource.spi.SecurityException;

/**
 * The class PasswordCredential is a placeholder for username and password.
 */

public class PasswordCredential implements java.io.Serializable {

    private String userName;
    private char[] password;

    private ManagedConnectionFactory mcf = null;

    /**
     * Constructor, creates a new password credential
     */
    public PasswordCredential( String userName, char[] password )  {
        this.userName = userName;
	this.password = password;
    }

    /**
     * Returns the username
     * @return Username
     */
    public String getUserName() {
        return userName;
    }

    /**
     * Returns the password
     * @return password
     */
    public char[] getPassword() {
        return password;
    }

    /**
     * Get the managed connection factory associated with this username
     * password pair.
     */
    public ManagedConnectionFactory getManagedConnectionFactory() {
        return mcf;
    }

    /**
     * Set the managed connection factory associated with this username
     * password pair.
     */
    public void setManagedConnectionFactory( ManagedConnectionFactory mcf ) {
        this.mcf = mcf;
    }

    /**
     * Tests object for equality
     */
    public boolean equals( Object other ) {
        if( getClass() != other.getClass() ) {
	    return false;
	}
	final PasswordCredential otherCredential = (PasswordCredential) other;
	return userName.equals( otherCredential.userName ) &&
	       java.util.Arrays.equals( password, otherCredential.password );
    }

    /**
     * Generates a hashCode for this object, hopefully this will be good
     * enough to spread out access....
     */
    public int hashCode() {
        return new String( password ).hashCode() << 5 + userName.hashCode();
    }
}
