/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.iiop.rmi.ir;

import org.omg.CORBA.IRObject;
import org.omg.CORBA.IRObjectOperations;

/**
 *  Interface of local IRObject implementations.
 *
 *  This defines the local (non-exported) methods.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
interface LocalIRObject
   extends IRObjectOperations
{
   /**
    *  Get an exported CORBA reference to this IRObject.
    */
   public IRObject getReference();

   /**
    *  Finalize the building process, and export.
    */
   public void allDone()
      throws IRConstructionException;

   /**
    *  Get a reference to the local IR implementation that
    *  this IR object exists in.
    */
   public RepositoryImpl getRepository();

   /**
    *  Unexport this object.
    */
   public void shutdown();
}

