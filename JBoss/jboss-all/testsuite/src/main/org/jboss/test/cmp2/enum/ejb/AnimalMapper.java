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
 * Maps AnimalEnum to Integer.
 *
 * @author <a href="mailto:gturner@unzane.com">Gerald Turner</a>
 */
public class AnimalMapper
   implements Mapper
{
   public Object toColumnValue(Object fieldValue)
   {
      return ((AnimalEnum)fieldValue).getOrdinal();
   }

   public Object toFieldValue(Object columnValue)
   {
      int ordinal = ((Integer)columnValue).intValue();
      return AnimalEnum.PENGUIN.valueOf(ordinal);
   }
}
