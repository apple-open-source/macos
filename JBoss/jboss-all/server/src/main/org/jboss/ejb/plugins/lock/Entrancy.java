/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.lock;

import java.io.Serializable;
import java.io.ObjectStreamException;

/**
 * This type safe enumeration s used to mark an invocation as non-entrant.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1.4.1 $
 */
public final class Entrancy implements Serializable
{
   public static final Entrancy ENTRANT = new Entrancy(true);
   public static final Entrancy NON_ENTRANT = new Entrancy(false);

   private final transient boolean value;
    
   private Entrancy(boolean value)
   {
      this.value = value;
   }

   public String toString()
   {
      if(value)
      {
         return "ENTRANT";
      } else
      {
         return "NON_ENTRANT";
      }
   }

   Object readResolve() throws ObjectStreamException
   {
      if(value)
      {
         return ENTRANT;
      } else
      {
         return NON_ENTRANT;
      }
   }

}

