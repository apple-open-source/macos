/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.iiop.rmi.ir;

import org.omg.CORBA.ContainedOperations;

/**
 *  Interface of local contained IR objects.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
interface LocalContained
   extends ContainedOperations, LocalIRObject
{
   /**
    *  Get a reference to the local IR implementation that
    *  this Contained object exists in.
    */
   public RepositoryImpl getRepository();
}

