/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.stream;

import java.io.ObjectOutputStream;
import java.io.IOException;

/**
 * An <code>ObjectOutputStream</code> that will auto reset after <i>n</i>
 * objects have been written to the underlying stream.
 *
 * <h3>Concurrency</h3>
 * This class is <b>not</b> synchronized.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class AutoResetObjectOutputStream
   extends ObjectOutputStreamAdapter
{
   /** Number of objects to write before resetting. */
   protected int after; // = 0

   /** Number of objects written so far. */
   protected int count; // = 0

   /**
    * Construct a new AutoResetObjectOutputStream.
    *
    * @param out     An ObjectOutputStream stream.
    * @param after   Number of objects to write before resetting.
    *
    * @throws IllegalArgumentException    After <= 0
    * @throws IOException                 Any exception thrown by
    *                                     the underlying OutputStream.
    */
   public AutoResetObjectOutputStream(ObjectOutputStream out, int after)
      throws IOException 
   {
      super(out);

      setResetAfter(after);
   }

   /**
    * Set the number of objects that must be written before resetting
    * the stream.
    *
    * @param after   Number of objects to write before resetting.
    *
    * @throws IllegalArgumentException    After <= 0
    */
   public void setResetAfter(int after) {
      if (after <= 0)
         throw new IllegalArgumentException("after <= 0");

      this.after = after;
   }

   /**
    * Get the number of objects that must be written before resetting
    * the stream.
    *
    * @return  Number of objects to write before resetting.
    */
   public final int getResetAfter() {
      return after;
   }

   /**
    * Get the number of objects written to the stream so far.
    *
    * @return  The number of objects written to the stream so far.
    */
   public final int getCount() {
      return count;
   }

   /**
    * Write the given object and reset if the number of objects written
    * (including this one) exceeds the after count.
    *
    * @param obj     Object to write.
    *
    * @throws IOException  Any exception thrown by the underlying stream.
    */
   protected void writeObjectOverride(Object obj) throws IOException {
      super.writeObjectOverride(obj);
      count++;

      if (count >= after) {
         reset();
      }
   }

   /**
    * Resets the object counter as well as the nested stream.
    *
    * @throws IOException
    */
   public void reset() throws IOException {
      out.reset();
      count = 0;
   }
}
