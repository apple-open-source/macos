/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Hashtable;
import java.util.List;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.J2EEDomain J2EEDomain}.
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.2 $
 *   
 * @jmx:mbean description="sample for jboss xmbean.dtd"
 *            persistPolicy="Never"
 *            persistPeriod="10"
 *            persistLocation="pl1"
 *            persistName="JBossXMLExample1"
 *            currencyTimeLimit="10"
 *            descriptor="name=\"testdescriptor\" value=\"testvalue\""
 *            state-action-on-update="RESTART"
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>20011126 Andreas Schaefer:</b>
 * <ul>
 * <li> Adjustments to the JBoss Guidelines
 * </ul>
 *
 * <p><b>20020304 Andreas Schaefer:</b>
 * <ul>
 * <li> Moved to XMBean
 * </ul>
 **/
public class J2EEDomainTarget
   extends J2EEManagedObject
//   implements J2EEDomainTargetMBean
{
   // -------------------------------------------------------------------------
   // Members
   // -------------------------------------------------------------------------
   
   private List mServers = new ArrayList();
   
   // -------------------------------------------------------------------------
   // Constructors
   // -------------------------------------------------------------------------
   
   /**
    *
    * @jmx:managed-constructor
    **/
   public J2EEDomainTarget( String pDomainName )
      throws
         MalformedObjectNameException,
         InvalidParentException
   {
      super( pDomainName, "J2EEDomain", "Manager" );
   }
   
   // -------------------------------------------------------------------------
   // Properties (Getters/Setters)
   // -------------------------------------------------------------------------
   
   /**
    * @jmx:managed-attribute description="List of all Servers on this Managment Domain"
    *            access="READ"
    *            persistPolicy="Never"
    *            persistPeriod="30"
    *            currencyTimeLimit="30"
    **/
   public ObjectName[] getServers() {
      return (ObjectName[]) mServers.toArray( new ObjectName[ 0 ] );
   }
   
   /**
    * @jmx:managed-operation description="Returns the requested Server" impact="INFO"
    **/
   public ObjectName getServer( int pIndex ) {
      if( pIndex >= 0 && pIndex < mServers.size() ) {
         return (ObjectName) mServers.get( pIndex );
      }
      return null;
   }
   
   public String toString() {
      return "J2EEDomainTarget { " + super.toString() + " } [ " +
         ", servers: " + mServers +
         " ]";
   }
   
   /**
    * @jmx:managed-operation description="adds a new child of this Management Domain" impact="INFO"
    **/
   public void addChild( ObjectName pChild ) {
       String lType = J2EEManagedObject.getType( pChild );
      if( J2EEServer.J2EE_TYPE.equals( lType ) ) {
         mServers.add( pChild );
      }
  }
   
   /**
    * @jmx:managed-operation description="removes a new child of this Management Domain" impact="ACTION"
    **/
   public void removeChild( ObjectName pChild ) {
      String lType = J2EEManagedObject.getType( pChild );
      if( J2EEServer.J2EE_TYPE.equals( lType ) ) {
         mServers.remove( pChild );
      }
   }
}
