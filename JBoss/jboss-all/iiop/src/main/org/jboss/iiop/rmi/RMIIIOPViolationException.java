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
public class RMIIIOPViolationException
   extends Exception
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   public RMIIIOPViolationException(String msg)
   {
      super(msg);
   }

   public RMIIIOPViolationException(String msg, String section)
   {
      this(msg);
      this.section = section;
   }

   // Public --------------------------------------------------------

   /**
    *  Return the section violated.
    */
   public String getSection()
   {
      return section;
   }

   // Z implementation ----------------------------------------------

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   /**
    *  The section violated.
    */
   private String section;

   // Inner classes -------------------------------------------------
}

