/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import java.util.Hashtable;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;

/**
 * Root class of the JBoss JSR-77 J2EEResources
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.3.2.1 $
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEManagedObjectMBean"
 */
public abstract class J2EEResource
   extends J2EEManagedObject
   implements J2EEResourceMBean
{
   /**
    * @param type the j2eeType key value
    * @param name Name of the J2EEResource
    * @param parentName the object name of the parent resource
    *
    * @throws InvalidParameterException If list of nodes or ports was null or empty
    */
   public J2EEResource( String type, String name, ObjectName parentName )
      throws
         MalformedObjectNameException,
         InvalidParentException
   {
      super( type, name, parentName );
   }

   // Protected -----------------------------------------------------
   
   /** Extract the name attribute from parent and return J2EEServer=name
    * @param parentName , the 
    * @return A hashtable with the J2EE Server name
    */
   protected Hashtable getParentKeys( ObjectName parentName )
   {
      Hashtable keys = new Hashtable();
      Hashtable lProperties = parentName.getKeyPropertyList();
      keys.put( J2EEServer.J2EE_TYPE, lProperties.get( "name" ) );

      return keys;
   }
}
