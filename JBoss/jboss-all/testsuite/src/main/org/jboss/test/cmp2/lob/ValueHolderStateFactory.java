/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.lob;

import org.jboss.ejb.plugins.cmp.jdbc.CMPFieldStateFactory;

/**
 *
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public class ValueHolderStateFactory
   implements CMPFieldStateFactory
{
   public Object getFieldState(Object fieldValue)
   {
      return fieldValue;
   }

   public boolean isStateValid(Object state, Object fieldValue)
   {
      boolean valid;
      if(state == null && fieldValue != null
         || state != null && fieldValue == null)
      {
         valid = false;
      }
      else
      {
         valid = (fieldValue == null ? true : !((ValueHolder)fieldValue).isDirty());
      }
      return valid;
   }
}
