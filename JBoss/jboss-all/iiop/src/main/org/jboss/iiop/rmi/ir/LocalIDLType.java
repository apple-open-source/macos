/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.iiop.rmi.ir;

import org.omg.CORBA.IDLTypeOperations;

/**
 *  Interface of local IDL types.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
interface LocalIDLType
   extends IDLTypeOperations, LocalIRObject
{
}

