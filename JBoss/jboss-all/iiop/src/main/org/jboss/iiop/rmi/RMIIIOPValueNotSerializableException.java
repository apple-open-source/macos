/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.iiop.rmi;


/**
 *  Exception denoting an RMI/IIOP subset violation.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2 $
 */
public class RMIIIOPValueNotSerializableException
   extends RMIIIOPViolationException
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   public RMIIIOPValueNotSerializableException(String msg)
   {
      super(msg);
   }

   public RMIIIOPValueNotSerializableException(String msg, String section)
   {
      super(msg, section);
   }

   // Public --------------------------------------------------------

   // Z implementation ----------------------------------------------

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}

