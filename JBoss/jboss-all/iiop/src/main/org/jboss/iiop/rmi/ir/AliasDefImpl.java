/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.iiop.rmi.ir;

import org.omg.CORBA.TypeCode;
import org.omg.CORBA.TypeCodePackage.BadKind;
import org.omg.CORBA.TCKind;
import org.omg.CORBA.IRObject;
import org.omg.CORBA.IDLType;
import org.omg.CORBA.IDLTypeHelper;
import org.omg.CORBA.IDLTypeOperations;
import org.omg.CORBA.DefinitionKind;
import org.omg.CORBA.AliasDef;
import org.omg.CORBA.AliasDefOperations;
import org.omg.CORBA.AliasDefHelper;
import org.omg.CORBA.AliasDefPOATie;
import org.omg.CORBA.Any;
import org.omg.CORBA.TypeDescription;
import org.omg.CORBA.TypeDescriptionHelper;
import org.omg.CORBA.ContainedOperations;
import org.omg.CORBA.ContainedPackage.Description;
import org.omg.CORBA.BAD_INV_ORDER;

import java.util.Map;
import java.util.HashMap;

/**
 *  AliasDef IR object.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
class AliasDefImpl
   extends TypedefDefImpl
   implements AliasDefOperations
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   AliasDefImpl(String id, String name, String version,
                LocalContainer defined_in,
                TypeCode typeCode, RepositoryImpl repository)
   {
      super(id, name, version, defined_in, typeCode,
            DefinitionKind.dk_Alias, repository);
   }

   // Public --------------------------------------------------------

   // LocalIRObject implementation ---------------------------------
 
   public IRObject getReference()
   {
      if (ref == null) {
         ref = org.omg.CORBA.AliasDefHelper.narrow(
                                servantToReference(new AliasDefPOATie(this)) );
      }
      return ref;
   }

   public void allDone()
      throws IRConstructionException
   {
      // Get my original type definition: It should have been created now.
      try {
         original_type_def = IDLTypeImpl.getIDLType(type().content_type(),
                                                    repository);
      } catch (BadKind ex) {
         throw new RuntimeException("Bad kind " + type().kind().value() +
                                    " for TypeCode.content_type()");
      }
 
      getReference();
   }

   // AliasDefOperations implementation -------------------------------

   public IDLType original_type_def()
   {
      return IDLTypeHelper.narrow(original_type_def.getReference());
   }

   public void original_type_def(IDLType arg)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   // Package protected ---------------------------------------------

   // Private -------------------------------------------------------

   /**
    *  My CORBA reference.
    */
   private AliasDef ref = null;

   /**
    *  My original IDL type.
    */
   private LocalIDLType original_type_def;
}
