/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.enum.ejb;

import org.jboss.ejb.plugins.cmp.jdbc.Mapper;


/**
 * org.jboss.ejb.plugins.cmp.jdbc.Mapper implementation.
 * Maps ColorEnum to Integer.
 *
 * @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
 */
public class ColorMapper
   implements Mapper
{
   public Object toColumnValue(Object fieldValue)
   {
      return ((ColorEnum)fieldValue).getOrdinal();
   }

   public Object toFieldValue(Object columnValue)
   {
      int ordinal = ((Integer)columnValue).intValue();
      return ColorEnum.RED.valueOf(ordinal);
   }
}
