package org.jboss.ejb;

/** A sample mbean that looks up the Container MBean for the jndiName passed
 to the inspectEJB method and retrieves its Home and Remote interfaces and
 lists all of their methods.

 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.1 $
 */
public interface ContainerRelectionMBean
{
   /** Lookup the mbean located under the object name ":service=Container,jndiName=<jndiName>"
    and invoke the getHome and getRemote interfaces and dump the methods for each
    in an html pre block.
    */
   public String inspectEJB(String jndiName);
}
