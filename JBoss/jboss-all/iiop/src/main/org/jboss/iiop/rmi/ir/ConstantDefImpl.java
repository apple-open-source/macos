/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.iiop.rmi.ir;

import org.omg.CORBA.Any;
import org.omg.CORBA.TypeCode;
import org.omg.CORBA.IRObject;
import org.omg.CORBA.ContainedOperations;
import org.omg.CORBA.ContainedPackage.Description;
import org.omg.CORBA.DefinitionKind;
import org.omg.CORBA.IDLType;
import org.omg.CORBA.IDLTypeHelper;
import org.omg.CORBA.ConstantDef;
import org.omg.CORBA.ConstantDefOperations;
import org.omg.CORBA.ConstantDescription;
import org.omg.CORBA.ConstantDescriptionHelper;
import org.omg.CORBA.ConstantDefPOATie;
import org.omg.CORBA.BAD_INV_ORDER;

/**
 *  Constant IR object.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.3 $
 */
public class ConstantDefImpl
   extends ContainedImpl
   implements ConstantDefOperations
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   private static final org.jboss.logging.Logger logger = 
               org.jboss.logging.Logger.getLogger(ConstantDefImpl.class);

   // Constructors --------------------------------------------------

   ConstantDefImpl(String id, String name, String version,
                    TypeCode typeCode, Any value,
                    LocalContainer defined_in, RepositoryImpl repository)
   {
      super(id, name, version, defined_in,
            DefinitionKind.dk_Constant, repository);

      this.typeCode = typeCode;
      this.value = value;
   }

   // Public --------------------------------------------------------


   // LocalIRObject implementation ---------------------------------

   public IRObject getReference()
   {
      if (ref == null) {
         ref = org.omg.CORBA.ConstantDefHelper.narrow(
                            servantToReference(new ConstantDefPOATie(this)) );
      }
      return ref;
   }

   public void allDone()
      throws IRConstructionException
   {
      // Get my type definition: It should have been created now.
      type_def = IDLTypeImpl.getIDLType(typeCode, repository);
      if (type_def == null)
         logger.debug("Got type_def: [NULL]");
      else
         logger.debug("Got type_def: [" + type_def.toString() + "]");

      getReference();
   }


   // ConstantDefOperations implementation ----------------------------

   public TypeCode type()
   {
      logger.debug("ConstantDefImpl.type(): entered.");
      return typeCode;
   }

   public IDLType type_def()
   {
      logger.debug("ConstantDefImpl.type_def(): entered.");
      try {
         return IDLTypeHelper.narrow(type_def.getReference());
      } finally {
         logger.debug("ConstantDefImpl.type_def(): returning.");
      }
   }

   public void type_def(IDLType arg)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }

   public Any value()
   {
      logger.debug("ConstantDefImpl.value(): entered.");
      return value;
   }

   public void value(Any arg)
   {
      throw new BAD_INV_ORDER("Cannot change RMI/IIOP mapping.");
   }


   // ContainedImpl implementation ----------------------------------

   public Description describe()
   {
      logger.debug("ConstantDefImpl.describe(): entered.");
      String defined_in_id = "IR";
 
      if (defined_in instanceof ContainedOperations)
         defined_in_id = ((ContainedOperations)defined_in).id();
 
      ConstantDescription d =
                   new ConstantDescription(name, id, defined_in_id, version,
                                            typeCode, value);
 
      Any any = getORB().create_any();

      ConstantDescriptionHelper.insert(any, d);

      return new Description(DefinitionKind.dk_Constant, any);
   }

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   /**
    *  My CORBA reference.
    */
   private ConstantDef ref = null;


   /**
    *  My TypeCode.
    */
   private TypeCode typeCode;

   /**
    *  My type definition.
    */
   private LocalIDLType type_def;

   /**
    *  My value.
    */
   private Any value;


   // Inner classes -------------------------------------------------
}
