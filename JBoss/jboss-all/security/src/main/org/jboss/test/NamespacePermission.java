package org.jboss.test;

import java.security.BasicPermission;
import java.security.Permission;
import java.security.PermissionCollection;
import javax.naming.Name;

/** A path like heirarchical permission.

@author Scott.Stark@jboss.org
@version $Revsiion:$
*/
public class NamespacePermission extends BasicPermission
{
    private PermissionName fullName;
    private String actions;

    /** Creates new NamespacePermission */
    public NamespacePermission(String name, String actions)
    {
        super(name, actions);
        this.actions = actions;
        fullName = new PermissionName(name);
    }
    public NamespacePermission(Name name, String actions)
    {
        super(name.toString(), actions);
        this.actions = actions;
        fullName = new PermissionName(name);
    }

    public String getActions()
    {
        return actions;
    }

    public PermissionName getFullName()
    {
        return fullName;
    }

    public boolean implies(Permission p)
    {
        String pactions = p.getActions();
        boolean implied = true;
        for(int n = 0; n < actions.length(); n ++)
        {
            char a = actions.charAt(n);
            char pa = pactions.charAt(n);
            if( (a != '-' && pa != '-' && pa != a) )
            {
                implied = false;
                break;
            }
            else if( a == '-' && pa != '-' )
            {
                implied = false;
                break;
            }
        }
        return implied;
    }

    public PermissionCollection newPermissionCollection()
    {
    	return new NamespacePermissionCollection();
    }
}
