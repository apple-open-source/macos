/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security;

import java.security.Principal;

/** An implementation of Principal and Comparable that represents any role.
Any Principal or name of a Principal when compared to an AnybodyPrincipal
using {@link #equals(Object) equals} or {@link #compareTo(Object) compareTo} 
will always be found equals to the AnybodyPrincipal.

Note that this class is not likely to operate correctly in a collection
since the hashCode() and equals() methods are not correlated.

@author Scott.Stark@jboss.org
@version $Revision: 1.5 $
*/
public class AnybodyPrincipal implements Comparable, Principal
{
    public static final String ANYBODY = "<ANYBODY>";
    public static final AnybodyPrincipal ANYBODY_PRINCIPAL = new AnybodyPrincipal();

    public int hashCode()
    {
        return ANYBODY.hashCode();
    }

    /**
    @return "<ANYBODY>"
    */
    public String getName()
    {
        return ANYBODY;
    }

    public String toString()
    {
        return ANYBODY;
    }
    
    /** This method always returns 0 to indicate equality for any argument.
    This is only meaningful when comparing against other Principal objects
     or names of Principals.

    @return true to indicate equality for any argument.
    */
    public boolean equals(Object another)
    {
        return true;
    }

    /** This method always returns 0 to indicate equality for any argument.
    This is only meaningful when comparing against other Principal objects
     or names of Principals.

    @return 0 to indicate equality for any argument.
    */
    public int compareTo(Object o)
    {
        return 0;
    }

}
