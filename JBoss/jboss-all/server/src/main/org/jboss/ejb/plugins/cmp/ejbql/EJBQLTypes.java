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
 * @version $Revision: 1.1 $
 */                            
public class EJBQLTypes {
   public static final int UNKNOWN_TYPE = -1;
   public static final int NUMERIC_TYPE = 1;
   public static final int STRING_TYPE = 2;
   public static final int DATETIME_TYPE = 3;
   public static final int BOOLEAN_TYPE = 4;
   public static final int ENTITY_TYPE = 5;
   public static final int VALUE_CLASS_TYPE = 6;
   
   public static int getEJBQLType(Class type) {
      if(type == Character.class || type == Character.TYPE ||
            type == Byte.class || type == Byte.TYPE ||
            type == Short.class || type == Short.TYPE ||
            type == Integer.class || type == Integer.TYPE ||
            type == Long.class || type == Long.TYPE ||
            type == Float.class || type == Float.TYPE ||
            type == Double.class || type == Double.TYPE) {
         return NUMERIC_TYPE;
      }
      if(type == String.class) {
         return STRING_TYPE;
      }
      if(Date.class.isAssignableFrom(type)) {
         return DATETIME_TYPE;
      }
      if(type == Boolean.class || type == Boolean.TYPE) {
         return BOOLEAN_TYPE;
      }
      if(EJBObject.class.isAssignableFrom(type) ||
           EJBLocalObject.class.isAssignableFrom(type)) {
         return ENTITY_TYPE;
      }
      return VALUE_CLASS_TYPE;
   }
}
