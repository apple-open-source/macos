/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.iiop.rmi.ir;

import org.omg.CORBA.ORB;
import org.omg.CORBA.IRObject;
import org.omg.CORBA.IRObjectOperations;
import org.omg.CORBA.DefinitionKind;
import org.omg.CORBA.BAD_INV_ORDER;
import org.omg.CORBA.UserException;
import org.omg.CORBA.CompletionStatus;
import org.omg.PortableServer.POA;
import org.omg.PortableServer.Servant;
import org.omg.PortableServer.POAPackage.WrongPolicy;
import org.omg.PortableServer.POAPackage.ServantAlreadyActive;
import org.omg.PortableServer.POAPackage.ObjectAlreadyActive;
import org.omg.PortableServer.POAPackage.ObjectNotActive;
//import org.omg.PortableServer.POAPackage.ServantNotActive;

/**
 *  Abstract base class for all IR object implementations.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.5 $
 */
abstract class IRObjectImpl
   implements IRObjectOperations
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   protected RepositoryImpl repository;
   protected DefinitionKind def_kind;
   

   // Static --------------------------------------------------------

   private static final org.jboss.logging.Logger logger = 
               org.jboss.logging.Logger.getLogger(IRObjectImpl.class);

   // Constructors --------------------------------------------------

   IRObjectImpl(DefinitionKind def_kind, RepositoryImpl repository)
   {
      this.def_kind = def_kind;
      this.repository = repository;
   }

   // Public --------------------------------------------------------

   // IRObjectOperations implementation -----------------------------

   public DefinitionKind def_kind()
   {
      logger.trace("IRObjectImpl.def_kind() entered.");
      return def_kind;
   }

   public void destroy()
   {
      throw new BAD_INV_ORDER("Cannot destroy RMI/IIOP mapping.", 2,
                              CompletionStatus.COMPLETED_NO);
   }

   // LocalIRObject implementation ----------------------------------

   abstract public IRObject getReference();

   public void allDone()
      throws IRConstructionException
   {
      getReference();
   }

   /**
    *  Unexport this object.
    */
   public void shutdown()
   {
      POA poa = getPOA();

      try {
         poa.deactivate_object(poa.reference_to_id(getReference()));
      } catch (UserException ex) {
         logger.warn("Could not deactivate IR object", ex);
      }
   }

   public RepositoryImpl getRepository()
   {
      return repository;
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   /**
    *  Return the ORB for this IRObject.
    */
   protected ORB getORB()
   {
      return repository.orb;
   }

   /**
    *  Return the POA for this IRObject.
    */
   protected POA getPOA()
   {
      return repository.poa;
   }

   /**
    *  Return the POA object ID of this IR object.
    */
   protected abstract byte[] getObjectId();

   /**
    *  Convert a servant to a reference.
    */
   protected org.omg.CORBA.Object servantToReference(Servant servant)
   {
      byte[] id = getObjectId();

      try {
//         repository.poa.activate_object(servant);
//         return repository.poa.servant_to_reference(servant);

//         repository.poa.activate_object_with_id(getObjectId(), servant);
//         return repository.poa.id_to_reference(getObjectId());

         logger.debug("#### IRObject.srv2ref: id=[" + new String(id) + "]");
         repository.poa.activate_object_with_id(id, servant);
         org.omg.CORBA.Object ref = repository.poa.id_to_reference(id);
         logger.debug("#### IRObject.srv2ref: returning ref.");
         //return repository.poa.id_to_reference(id);
         return ref;
      } catch (WrongPolicy ex) {
         logger.debug("Exception converting CORBA servant to reference", ex);
      } catch (ServantAlreadyActive ex) {
         logger.debug("Exception converting CORBA servant to reference", ex);
      } catch (ObjectAlreadyActive ex) {
         logger.debug("Exception converting CORBA servant to reference", ex);
      } catch (ObjectNotActive ex) {
         logger.debug("Exception converting CORBA servant to reference", ex);
//      } catch (ServantNotActive ex) {
//         logger.debug("Exception converting CORBA servant to reference", ex);
      }
      return null;
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
