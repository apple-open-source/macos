/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.metadata;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;

/** The meta data object for the security-identity element.
The security-identity element specifies whether the caller’s security
identity is to be used for the execution of the methods of the enterprise
bean or whether a specific run-as role is to be used. It
contains an optional description and a specification of the security
identity to be used.

Used in: session, entity, message-driven

@author <a href="mailto:Scott_Stark@displayscape.com">Scott Stark</a>.
@version $Revision: 1.6 $
*/
public class SecurityIdentityMetaData extends MetaData
{
    private String description;
    /** The use-caller-identity element specifies that the caller’s security
    identity be used as the security identity for the execution of the
    enterprise bean’s methods.
    */
    private boolean useCallerIdentity;
    /** The run-as/role-name element specifies the run-as security role name
    to be used for the execution of the methods of an enterprise bean.
    */
	private String runAsRoleName;

	public String getDescription()
    {
        return description;
    }

    /**
     */
    public boolean getUseCallerIdentity()
    {
        return useCallerIdentity;
    }
    /** Get the run-as security identity. This is the principal to be used for
     the run-as identity of an enterprise bean in terms of its security role.
     */
    public String getRunAsRoleName()
    {
        return runAsRoleName;
    }

    /**
     @param element, the security-identity element from the ejb-jar
     */
    public void importEjbJarXml(Element element) throws DeploymentException
    {
		description = getElementContent(getOptionalChild(element, "description"));
        Element callerIdent = getOptionalChild(element, "use-caller-identity");
        Element runAs = getOptionalChild(element, "run-as");
        if( callerIdent == null && runAs == null )
            throw new DeploymentException("security-identity: either use-caller-identity or run-as must be specified");
        if( callerIdent != null && runAs != null )
            throw new DeploymentException("security-identity: only one of use-caller-identity or run-as can be specified");
        if( callerIdent != null )
        {
            useCallerIdentity = true;
        }
        else
        {
            runAsRoleName = getElementContent(getUniqueChild(runAs, "role-name"));
        }
	}

}
