/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.ejbql;

import java.util.Date;
import javax.ejb.EJBLocalObject;
import javax.ejb.EJBObject;

/**
 * This class contains a list of the reconized EJB-QL types.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 * @version $Revision: 1.1.4.2 $
 */
public final class EJBQLTypes
{
   public static final int UNKNOWN_TYPE = -1;
   public static final int NUMERIC_TYPE = 1;
   public static final int STRING_TYPE = 2;
   public static final int DATETIME_TYPE = 3;
   public static final int BOOLEAN_TYPE = 4;
   public static final int ENTITY_TYPE = 5;
   public static final int VALUE_CLASS_TYPE = 6;

   public static int getEJBQLType(Class type)
   {
      int result;
      if(type == Boolean.class || type == Boolean.TYPE)
      {
         result = BOOLEAN_TYPE;
      }
      else if(type.isPrimitive()
         || type == Character.class
         || Number.class.isAssignableFrom(type))
      {
         result = NUMERIC_TYPE;
      }
      else if(type == String.class)
      {
         result = STRING_TYPE;
      }
      else if(Date.class.isAssignableFrom(type))
      {
         result = DATETIME_TYPE;
      }
      else if(EJBObject.class.isAssignableFrom(type) ||
         EJBLocalObject.class.isAssignableFrom(type))
      {
         result = ENTITY_TYPE;
      }
      else
      {
         result = VALUE_CLASS_TYPE;
      }
      return result;
   }
}
