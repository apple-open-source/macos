/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.proxy.ejb;

import java.io.Externalizable;
import java.io.ObjectOutput;
import java.io.ObjectInput;
import java.io.IOException;
import java.util.ArrayList;

/**
 * A result of get-method invocation of CMP 2.0 entity bean in the case where read ahead is turned on.
 * Usage: on server set main result via {@link #setMainResult(java.lang.Object)} and add ahead results via
 * {@link #addAheadResult(java.lang.Object)}. On client get main result via {@link #getMainResult()} and
 * array of ahead results via {@link #getAheadResults()}.
 *
 * @author <a href="mailto:on@ibis.odessa.ua">Oleg Nitz</a>
 * @version $Revision: 1.1.4.1 $
 */
public class ReadAheadResult
      implements Externalizable
{
   /** Serial Version Identifier. @since 1.1 */
   private static final long serialVersionUID = -4041516583763000658L;

   // Attributes ----------------------------------------------------

   /**
    * A List of read ahead values, during externalization is replaces by array
    */
   private ArrayList aheadList = new ArrayList();

   /**
    * A List of read ahead values, during externalization is replaces by array
    */
   private Object[] aheadArray;

   /**
    * A hash map of read ahead values, maps Methods to values.
    */
   private Object mainResult;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   public ReadAheadResult()
   {
   }

   // Static --------------------------------------------------------

   // Public --------------------------------------------------------

   public void setMainResult(Object mainResult)
   {
      this.mainResult = mainResult;
   }

   public void addAheadResult(Object aheadResult)
   {
      aheadList.add(aheadResult);
   }

   public Object getMainResult()
   {
      return mainResult;
   }

   public Object[] getAheadResult()
   {
      if (aheadArray == null)
      {
         aheadArray = aheadList.toArray(new Object[aheadList.size()]);
         aheadList = null;
      }
      return aheadArray;
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------


   // Private -------------------------------------------------------

   public void writeExternal(ObjectOutput out)
         throws IOException
   {
      out.writeObject(mainResult);
      out.writeObject(getAheadResult());
   }

   public void readExternal(ObjectInput in)
         throws IOException, ClassNotFoundException
   {
      mainResult = in.readObject();
      aheadArray = (Object[]) in.readObject();
   }

   // Inner classes -------------------------------------------------
}

