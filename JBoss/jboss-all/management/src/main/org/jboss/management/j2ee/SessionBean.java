/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;

/** The JBoss JSR-77.30.13 implementation of the SessionBean model
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.1 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.EJBMBean"
 */
public abstract class SessionBean
   extends EJB
   implements SessionBeanMBean
{

   /** Create a SessionBean model
    *
    * @param j2eeType the type of session bean
    * @param name the ejb name, currently the JNDI name
    * @param ejbModuleName the JSR-77 EJBModule name for this bean
    * @throws MalformedObjectNameException
    * @throws InvalidParentException
    */
   public SessionBean( String j2eeType, String name, ObjectName ejbModuleName,
      ObjectName ejbContainerName)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super( j2eeType, name, ejbModuleName, ejbContainerName );
   }

   // -------------------------------------------------------------------------
   // Properties (Getters/Setters)
   // -------------------------------------------------------------------------

}
