/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import java.security.InvalidParameterException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.J2EEModule J2EEModule}.
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.3.2.1 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEDeployedObjectMBean"
 */
public abstract class J2EEModule
      extends J2EEDeployedObject
      implements J2EEModuleMBean
{
   // Attributes ----------------------------------------------------

   private List mJVMs = new ArrayList();

   // Constructors --------------------------------------------------

   /**
    * Constructor taking the Name of this Object
    *
    * @param pName Name to be set which must not be null
    * @param pParent Object Name of its parent which can either
    *                be a J2EEApplication or J2EEServer if a
    *                standalone module (not packed into an EAR file)
    * @param pDeploymentDescriptor
    *
    * @throws InvalidParameterException If the given Name is null
    */
   public J2EEModule(
         String pType,
         String pName,
         ObjectName pParent,
         ObjectName[] pJVMs,
         String pDeploymentDescriptor
         )
         throws
         MalformedObjectNameException,
         InvalidParentException
   {
      super(pType, pName, pParent, pDeploymentDescriptor);
      mJVMs = new ArrayList(Arrays.asList(pJVMs == null ? new ObjectName[0] : pJVMs));
   }

   // Public --------------------------------------------------------

   /**
    * @jmx:managed-attribute
    */
   public ObjectName[] getJVMs()
   {
      return (ObjectName[]) mJVMs.toArray(new ObjectName[0]);
   }

   /**
    * @jmx:managed-operation
    */
   public ObjectName getJVM(int pIndex)
   {
      if (pIndex >= 0 && pIndex < mJVMs.size())
      {
         return (ObjectName) mJVMs.get(pIndex);
      }
      else
      {
         return null;
      }
   }
}
