/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.state;

import java.io.Serializable;

import org.jboss.util.CloneableObject;

/**
 * Provides the basic interface for states (both accepting, acceptable and final)
 * of a state machine.
 *
 * <p>Each state is immutable and has a name and integer value.  States are
 *    considered equivilent if the integer values equal.
 *
 * <p>Note that the second opperand is an annonymous class, changing its
 *    equivilence from a vanilla Type instance.
 *
 * <p>State objects also can contain an optional opaque object.  This is provided
 *    for applications to make use of the state machine for managing data
 *    assocciated with the state.
 *
 * @version <tt>$Revision: 1.3 $</tt>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class State
   extends CloneableObject
   implements Serializable
{
   /**
    * The integer value of the state.
    *
    * <p>States with the same value are equal.
    */
   public final int value;

   /** The human readable name of the state. */
   public final String name;

   /** The optional opaque user data for the state. */
   protected Object opaque;
   
   public State(final int value, final String name)
   {
      this.value = value;
      this.name = name;
   }

   public State(final int value)
   {
      this(value, null);
   }
   
   public int getValue()
   {
      return value;
   }

   /**
    * Returns the name of the state, or if null returns the
    * integer value as a string.
    */
   public String getName()
   {
      return (name == null ? String.valueOf(value) : name);
   }

   public void setOpaque(final Object obj)
   {
      opaque = obj;
   }

   public Object getOpaque()
   {
      return opaque;
   }
   
   public String toString()
   {
      return getName();
   }

   /**
    * Returns the identity string of this state.
    *
    * <p>This is the same value returned by {@link Object#toString()}.
    */
   public String toIdentityString()
   {
      return super.toString();
   }

   /**
    * Returns the has code of the object.
    *
    * @return The integer value of the state.
    */
   public int hashCode()
   {
      return value;
   }

   /**
    * Check for state equality.
    *
    * <p>States are considered equal if the integer values
    *    of the State objects are equivilent.
    */
   public boolean equals(final Object obj)
   {
      if (obj == this) return true;

      if (obj instanceof State) {
         return value == ((State)obj).getValue();
      }

      return false;
   }
}
