/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.metadata;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;

/** The meta data information for a resource-ref element.
 The resource-ref element contains a declaration of enterprise bean’s
 reference to an external resource. It consists of an optional description,
 the resource manager connection factory reference name, the
 indication of the resource manager connection factory type expected
 by the enterprise bean code, the type of authentication (Application
 or Container), and an optional specification of the shareability of
 connections obtained from the resource (Shareable or Unshareable).
 Used in: entity, message-driven, and session

 *   @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 *   @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>.
 *   @version $Revision: 1.8.4.1 $
 */
public class ResourceRefMetaData extends MetaData
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   /** The ejb-jar/../resource-ref/res-ref-name element used by the bean code.
    The res-ref-name element specifies the name of a resource manager con-nection
    factory reference. The name is a JNDI name relative to the
    java:comp/env context. The name must be unique within an enterprise
    bean.
    */
   private String refName;
   /** The jboss/../resource-ref/resource-name value that maps to a resource-manager */
   private String name;
   /** The jndi name of the deployed resource, or the URL in the case of
    a java.net.URL resource type. This comes from either the:
    jboss/../resource-ref/jndi-name element value or the
    jboss/../resource-ref/res-url element value or the
    jboss/../resource-manager/res-jndi-name element value
    jboss/../resource-manager/res-url element value
    */
   private String jndiName;
   /** The ejb-jar/../resource-ref/res-type element.
    The res-type element specifies the Java class or interface of the data source
    */
   private String type;
   /** The ejb-jar/../resource-ref/res-auth value.
    The res-auth element specifies whether the enterprise bean code signs
    on programmatically to the resource manager, or whether the Container
    will sign on to the resource manager on behalf of the enterprise bean.
    In the latter case, the Container uses information that is supplied by
    the Deployer.
    The value of this element must be one of the following for EJB2.0,
    Servlet 2.3:
    <res-auth>Application</res-auth>
    <res-auth>Container</res-auth>
    or for Servlet 2.2:
    <res-auth>CONTAINER</res-auth>
    <res-auth>SERVLET</res-auth>
    */
   private boolean containerAuth;
   /** The ejb-jar/../resource-ref/res-sharing-scope value
    The res-sharing-scope element specifies whether connections obtained
    through the given resource manager connection factory reference can
    be shared. The value of this element, if specified, must be one of the
    two following:
    <res-sharing-scope>Shareable</res-sharing-scope>
    <res-sharing-scope>Unshareable</res-sharing-scope>
    The default value is Shareable.
    */
   private boolean isShareable;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   public ResourceRefMetaData()
   {
   }

   // Public --------------------------------------------------------

   public String getRefName()
   {
      return refName;
   }

   public String getResourceName()
   {
      if (name == null)
      {
         // default is refName
         name = refName;
      }
      return name;
   }

   public void setResourceName(String resName)
   {
      name = resName;
   }

   public String getJndiName()
   {
      return jndiName;
   }

   public String getType()
   {
      return type;
   }

   public boolean isContainerAuth()
   {
      return containerAuth;
   }

   public boolean isShareable()
   {
      return isShareable;
   }

   public void importEjbJarXml(Element element) throws DeploymentException
   {
      refName = getElementContent(getUniqueChild(element, "res-ref-name"));

      type = getElementContent(getUniqueChild(element, "res-type"));

      String auth = getElementContent(getUniqueChild(element, "res-auth"));
      if (auth.equalsIgnoreCase("Container"))
      {
         containerAuth = true;
      }
      else if (auth.equals("Application") || auth.equals("SERVLET") )
      {
         containerAuth = false;
      }
      else
      {
         throw new DeploymentException("res-auth tag should be 'Container' or "
            + "'Application' or 'SERVLET'");
      }
      // The res-sharing-scope element
      String sharing = getElementContent(getOptionalChild(element, "res-sharing-scope"), "Shareable");
      isShareable = sharing.equals("Shareable");
   }

   public void importJbossXml(Element element) throws DeploymentException
   {
      // Look for the resource-ref/resource-name element
      Element child = getOptionalChild(element, "resource-name");
      if (child == null)
      {
         // There must be a resource-ref/res-url value if this is a URL resource
         if (type.equals("java.net.URL"))
            jndiName = getElementContent(getUniqueChild(element, "res-url"));
         // There must be a resource-ref/jndi-name value otherwise
         else
            jndiName = getElementContent(getUniqueChild(element, "jndi-name"));
      }
      else
      {
         name = getElementContent(child);
      }
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
