/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.security.test;

import java.io.Serializable;
import java.security.BasicPermission;
import java.util.Comparator;
import java.util.Properties;
import javax.naming.CompoundName;
import javax.naming.Name;
import javax.naming.NamingException;

/** A javax.naming.Name based key class used as the name attribute
by NamespacePermissions.

@author Scott.Stark@jboss.org
@version $Revision: 1.3 $
*/
public class PermissionName implements Comparable, Serializable
{
	/** The Properties used for the project directory heirarchical names */
	static Name emptyName;
	static Properties nameSyntax = new Properties();
	static
	{
		nameSyntax.put("jndi.syntax.direction", "left_to_right");
		nameSyntax.put("jndi.syntax.separator", "/");
		try
		{
			emptyName = new CompoundName("", nameSyntax);
		}
		catch(NamingException e)
		{
		}	
	}
    private Name name;

    /** An alternate PermissionName comparator that first orders names by
        length(longer names before shorter names) to ensure that the most
        precise names are seen first.
    */
    public static class NameLengthComparator implements Comparator
    {
        public int compare(Object o1, Object o2)
        {
            PermissionName p1 = (PermissionName) o1;
            PermissionName p2 = (PermissionName) o2;
            // if p1 is longer than p2, its < p2 -> < 0
            int compare = p2.size() - p1.size();
            if( compare == 0 )
                compare = p1.compareTo(p2);
            return compare;
        }
    }

    /** Creates new NamespacePermission */
    public PermissionName(String name) throws IllegalArgumentException
    {
        try
        {
            this.name = new CompoundName(name, nameSyntax);
        }
        catch(NamingException e)
        {
            throw new IllegalArgumentException(e.toString(true));
        }
    }
    public PermissionName(Name name)
    {
        this.name = name;
    }

    public int compareTo(Object obj)
    {
        PermissionName pn = (PermissionName) obj;
        /* Each level must be compared. The first level to not be equals
         determines the ordering of the names.
        */
        int compare = name.size() - pn.name.size();
        int length = Math.min(name.size(), pn.name.size());
        for(int n = 0; compare == 0 && n < length; n ++)
        {
            String atom0 = name.get(n);
            String atom1 = pn.name.get(n);
            compare = atom0.compareTo(atom1);
        }
        return compare;
    }

    public boolean equals(Object obj)
    {
        return compareTo(obj) == 0;
    }

    public int hashCode()
    {
        return name.hashCode();
    }

    public int size()
    {
        return name.size();
    }

    public boolean isParent(PermissionName childName)
    {
        boolean isParent = childName.name.startsWith(name);
        return isParent;
    }

    public String toString()
    {
        return name.toString();
    }
}
