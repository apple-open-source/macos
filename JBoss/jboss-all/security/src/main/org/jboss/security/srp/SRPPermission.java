/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.srp;

import java.security.BasicPermission;

/** A custom permission class for protecting access to sensitive SRP information
like the private session key and private key.

The following table lists all the possible SRPPermission target names,
and for each provides a description of what the permission allows
and a discussion of the risks of granting code the permission.
<table border=1 cellpadding=5>
    <tr>
        <th>Permission Target Name</th>
        <th>What the Permission Allows</th>
        <th>Risks of Allowing this Permission</th>
    </tr>

    <tr>
        <td>getSessionKey</td>
        <td>Access the private SRP session key</td>
        <td>This provides access the the private session key that results from
the SRP negiotation. Access to this key will allow one to encrypt/decrypt msgs
that have been encrypted with the session key.
        </td>
    </tr>

</table>

@author Scott.Stark@jboss.org
@version $Revision: 1.2.4.1 $
*/
public class SRPPermission extends BasicPermission
{

    /** Creates new SRPPermission */
    public SRPPermission(String name)
    {
        super(name);
    }
    public SRPPermission(String name, String actions)
    {
        super(name, actions);
    }

}
