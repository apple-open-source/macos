
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.adapter; // Generated package name

import javax.transaction.xa.XAException;
import javax.transaction.xa.XAResource;
import javax.transaction.xa.Xid;


/**
 * TestXAResource.java
 *
 *
 * Created: Mon Dec 31 19:55:31 2001
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class TestXAResource 
 implements XAResource {
   public TestXAResource ()
   {
      
   }
   // implementation of javax.transaction.xa.XAResource interface

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void start(Xid param1, int param2) throws XAException
   {
      // TODO: implement this javax.transaction.xa.XAResource method
   }

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void end(Xid param1, int param2) throws XAException
   {
      // TODO: implement this javax.transaction.xa.XAResource method
   }

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void commit(Xid param1, boolean param2) throws XAException
   {
      // TODO: implement this javax.transaction.xa.XAResource method
   }

   /**
    *
    * @param param1 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void rollback(Xid param1) throws XAException
   {
      // TODO: implement this javax.transaction.xa.XAResource method
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public int prepare(Xid param1) throws XAException
   {
      // TODO: implement this javax.transaction.xa.XAResource method
      return 0;
   }

   /**
    *
    * @param param1 <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public void forget(Xid param1) throws XAException
   {
      // TODO: implement this javax.transaction.xa.XAResource method
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public Xid[] recover(int param1) throws XAException
   {
      // TODO: implement this javax.transaction.xa.XAResource method
      return null;
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public boolean isSameRM(XAResource param1) throws XAException
   {
      // TODO: implement this javax.transaction.xa.XAResource method
      return false;
   }

   /**
    *
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public int getTransactionTimeout() throws XAException
   {
      // TODO: implement this javax.transaction.xa.XAResource method
      return 0;
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.transaction.xa.XAException <description>
    */
   public boolean setTransactionTimeout(int param1) throws XAException
   {
      // TODO: implement this javax.transaction.xa.XAResource method
      return false;
   }

}// TestXAResource
