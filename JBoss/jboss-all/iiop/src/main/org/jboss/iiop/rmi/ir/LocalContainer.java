/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.iiop.rmi.ir;

import org.omg.CORBA.ContainerOperations;
import org.omg.CORBA.DefinitionKind;

/**
 *  Interface of local containers.
 *  Those who delegate the container implementation to the
 *  ContainerImplDelegate should implement this interface.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
interface LocalContainer
   extends ContainerOperations, LocalIRObject
{
   /**
    *  Same as org.omg.CORBA.Contained.lookup(),
    *  but returns local objects instead.
    */
   public LocalContained _lookup(String search_name);

   /**
    *  Same as org.omg.CORBA.Contained.contents(),
    *  but returns local objects instead.
    */
   public LocalContained[] _contents(DefinitionKind limit_type,
                                     boolean exclude_inherited);

   /**
    *  Same as org.omg.CORBA.Contained.lookup_name(),
    *  but returns local objects instead.
    */
   public LocalContained[] _lookup_name(String search_name,
                                        int levels_to_search,
                                        DefinitionKind limit_type,
                                        boolean exclude_inherited);

   /**
    *  Add an entry to the delegating container.
    */
   public void add(String name, LocalContained contained)
      throws IRConstructionException;
}

