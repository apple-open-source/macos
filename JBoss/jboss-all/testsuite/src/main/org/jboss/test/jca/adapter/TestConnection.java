
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.jca.adapter;

import javax.resource.ResourceException;
import javax.resource.cci.Connection;
import javax.resource.cci.ConnectionMetaData;
import javax.resource.cci.Interaction;
import javax.resource.cci.LocalTransaction;
import javax.resource.cci.ResultSetInfo;




/**
 * TestConnection.java
 *
 *
 * Created: Sun Mar 10 19:35:48 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class TestConnection 
 implements Connection {

   private TestManagedConnection mc;
   private boolean mcIsNull = true;

   public TestConnection (TestManagedConnection mc)
   {
      this.mc = mc;
      mcIsNull = false;
   }

   public void setFailInPrepare(final boolean fail, final int xaCode)
   {
      mc.setFailInPrepare(fail, xaCode);
   }

   public void setFailInCommit(final boolean fail, final int xaCode)
   {
      mc.setFailInCommit(fail, xaCode);
   }

   public boolean isInTx()
   {
      return mc.isInTx();
   }

   void setMc(TestManagedConnection mc)
   {
      if (mc == null) 
      {
         mcIsNull = true;
      } // end of if ()
      else
      {
         this.mc = mc;
      } // end of else
   }


   // implementation of javax.resource.cci.Connection interface

   /**
    *
    * @exception javax.resource.ResourceException <description>
    */
   public void close()
   {
      mc.connectionClosed(this);
      mcIsNull = true;
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public Interaction createInteraction() throws ResourceException
   {
      // TODO: implement this javax.resource.cci.Connection method
      return null;
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public LocalTransaction getLocalTransaction() throws ResourceException
   {
      // TODO: implement this javax.resource.cci.Connection method
      return null;
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public ConnectionMetaData getMetaData() throws ResourceException
   {
      // TODO: implement this javax.resource.cci.Connection method
      return null;
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public ResultSetInfo getResultSetInfo() throws ResourceException
   {
      // TODO: implement this javax.resource.cci.Connection method
      return null;
   }

   /**
    * Similate a connection error
    */
   public void simulateConnectionError()
      throws Exception
   {
      Exception e = new Exception("Simulated exception");
      mc.connectionError(this, e);
      throw e;
   }
   
}// TestConnection

