/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.metadata;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;

/** The metadata object for the security-role-ref element.
The security-role-ref element contains the declaration of a security
role reference in the enterprise bean’s code. The declaration con-sists
of an optional description, the security role name used in the
code, and an optional link to a defined security role.
The value of the role-name element must be the String used as the
parameter to the EJBContext.isCallerInRole(String roleName) method.
The value of the role-link element must be the name of one of the
security roles defined in the security-role elements.

Used in: entity and session

 *   @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 *   @author <a href="mailto:Scott_Stark@displayscape.com">Scott Stark</a>.
 *   @version $Revision: 1.6 $
 */
public class SecurityRoleRefMetaData extends MetaData {
    // Constants -----------------------------------------------------
    
    // Attributes ----------------------------------------------------
	private String name;
    private String link;
    private String description;
	
    // Static --------------------------------------------------------
    
    // Constructors --------------------------------------------------
    public SecurityRoleRefMetaData () {
	}
	
    // Public --------------------------------------------------------
	
	public String getName() { return name; }
	
	public String getLink() { return link; }
	public String getDescription() { return description; }

    public void importEjbJarXml(Element element) throws DeploymentException {
		name = getElementContent(getUniqueChild(element, "role-name"));
		link = getElementContent(getOptionalChild(element, "role-link"));
		description = getElementContent(getOptionalChild(element, "description"));
	}		

}
