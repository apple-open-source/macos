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
import org.omg.CORBA.DefinitionKind;
import org.omg.CORBA.IDLType;
import org.omg.CORBA.IDLTypeHelper;
import org.omg.CORBA.IDLTypeOperations;

import java.io.UnsupportedEncodingException;

/**
 *  IDLType IR object.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.3.4.1 $
 */
abstract class IDLTypeImpl
   extends IRObjectImpl
   implements LocalIDLType
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   IDLTypeImpl(TypeCode typeCode, DefinitionKind def_kind,
               RepositoryImpl repository)
   {
      super(def_kind, repository);

      this.typeCode = typeCode;
   }

   // Public --------------------------------------------------------

   // IDLTypeOperations implementation ---------------------------------

   public TypeCode type()
   {
      return typeCode;
   }

   // Package protected ---------------------------------------------

   /**
    *  Return the LocalIDLType for the given TypeCode.
    */
   static LocalIDLType getIDLType(TypeCode typeCode, RepositoryImpl repository)
   {
      TCKind tcKind = typeCode.kind();

      if (PrimitiveDefImpl.isPrimitiveTCKind(tcKind))
         return new PrimitiveDefImpl(typeCode, repository);

      if (tcKind == TCKind.tk_sequence)
         return repository.getSequenceImpl(typeCode);

      if (tcKind == TCKind.tk_value || tcKind == TCKind.tk_value_box ||
          tcKind == TCKind.tk_alias || tcKind == TCKind.tk_struct || 
          tcKind == TCKind.tk_union || tcKind == TCKind.tk_enum ||
          tcKind == TCKind.tk_objref) {
         try {
            return (LocalIDLType)repository._lookup_id(typeCode.id());
         } catch (BadKind ex) {
            throw new RuntimeException("Bad kind for TypeCode.id()");
         }
      }

      throw new RuntimeException("TODO: tcKind=" + tcKind.value());
   }

   // Protected -----------------------------------------------------

   /**
    *  Return the POA object ID of this IR object.
    *  We delegate to the IR to get a serial number ID.
    */
   protected byte[] getObjectId()
   {
      return repository.getNextObjectId();
   }

   // Private -------------------------------------------------------

   /**
    *  My TypeCode.
    */
   private TypeCode typeCode;

}

