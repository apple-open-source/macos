/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.lob;

import java.io.Serializable;

/**
 *
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public class ValueHolder
   implements Serializable
{
   private String value;
   private boolean dirty;

   public ValueHolder(String value)
   {
      this.value = value;
   }

   public String getValue()
   {
      return value;
   }

   public void setValue(String value)
   {
      this.value = value;
   }

   public boolean isDirty()
   {
      return dirty;
   }

   public void setDirty(boolean dirty)
   {
      this.dirty = dirty;
   }

   public boolean equals(Object o)
   {
      return true;
   }
}
