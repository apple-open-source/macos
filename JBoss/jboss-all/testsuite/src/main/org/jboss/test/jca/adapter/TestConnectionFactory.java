
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.adapter; 

import javax.naming.NamingException;
import javax.naming.Reference;
import javax.resource.ResourceException;
import javax.resource.cci.Connection;
import javax.resource.cci.ConnectionFactory;
import javax.resource.cci.ConnectionSpec;
import javax.resource.cci.RecordFactory;
import javax.resource.cci.ResourceAdapterMetaData;
import javax.resource.spi.ConnectionManager;
import javax.resource.Referenceable;

// Generated package name
/**
 * TestConnectionFactory.java
 *
 *
 * Created: Tue Jan  1 01:02:16 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class TestConnectionFactory implements ConnectionFactory, Referenceable
{

   private final ConnectionManager cm;
   private final TestManagedConnectionFactory mcf;

   private Reference ref;

   public TestConnectionFactory (final ConnectionManager cm, final TestManagedConnectionFactory mcf)
   {
      this.cm = cm;
      this.mcf = mcf;
   }
   // implementation of javax.resource.Referenceable interface

      /**
    *
    * @param param1 <description>
    */
   public void setReference(Reference ref)
   {
      this.ref = ref;
   }
   // implementation of javax.naming.Referenceable interface

   /**
    *
    * @return <description>
    * @exception javax.naming.NamingException <description>
    */
   public Reference getReference() throws NamingException
   {
      return ref;
   }
   // implementation of javax.resource.cci.ConnectionFactory interface

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public Connection getConnection() throws ResourceException
   {
      return (Connection)cm.allocateConnection(mcf, null);
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public Connection getConnection(ConnectionSpec ignore) throws ResourceException
   {
      return (Connection)cm.allocateConnection(mcf, null);
   }

   public Connection getConnection(String failure) throws ResourceException
   {
      return (Connection)cm.allocateConnection(mcf, new TestConnectionRequestInfo(failure));
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public RecordFactory getRecordFactory() throws ResourceException
   {
     // TODO: implement this javax.resource.cci.ConnectionFactory method
     return null;
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public ResourceAdapterMetaData getMetaData() throws ResourceException
   {
     // TODO: implement this javax.resource.cci.ConnectionFactory method
     return null;
   }

   public void setFailure(String failure)
   {
      mcf.setFailure(failure);
   }

}
