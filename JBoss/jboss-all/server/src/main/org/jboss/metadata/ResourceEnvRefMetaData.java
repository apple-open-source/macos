/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.metadata;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;

/** The meta data information for a resource-env-ref element.
The resource-env-ref element contains a declaration of an enterprise
bean’s reference to an administered object associated with a resource
in the enterprise bean’s environment. It consists of an optional
description, the resource environment reference name, and an indication
of the resource environment reference type expected by the enterprise
bean code.

Used in: entity, message-driven and session

Example:
<resource-env-ref>
    <resource-env-ref-name>jms/StockQueue</resource-env-ref-name>
    <resource-env-ref-type>javax.jms.Queue</resource-env-ref-type>
</resource-env-ref>

@author <a href="mailto:Scott_Stark@displayscape.com">Scott Stark</a>.
@version $Revision: 1.4 $
*/
public class ResourceEnvRefMetaData extends MetaData
{
    /** The ejb-jar/../resource-env-ref/resource-env-ref-name element.
    The resource-env-ref-name element specifies the name of a resource
    environment reference; its value is the environment entry name used in
    the enterprise bean code. The name is a JNDI name relative to the
    java:comp/env context and must be unique within an enterprise bean.
    */
	private String refName;
    /** The jboss/../resource-env-ref/jndi-name element value. This is the
      jndi name of the deployed resource.
     */
    private String jndiName;
    /** The ejb-jar/../resource-env-ref/resource-env-ref-type java element.
     The res-type element specifies the Java class or interface of a resource
     environment reference
     */
    private String type;
	
	public String getRefName() { return refName; }
    public String getJndiName() { return jndiName; }
	public String getType() { return type; }

    /** Parse the ejb-jar child element
     @param element, the resource-env-ref element
     */
    public void importEjbJarXml(Element element) throws DeploymentException
    {
		refName = getElementContent(getUniqueChild(element, "resource-env-ref-name"));
		type = getElementContent(getUniqueChild(element, "resource-env-ref-type"));
	}

    /** Parse the jboss child element
     @param element, the resource-env-ref element
     */
	public void importJbossXml(Element element) throws DeploymentException
    {
        jndiName = getElementContent(getUniqueChild(element, "jndi-name"));
	}
}
