package org.jboss.ejb.plugins.cmp.jdbc;

import java.util.HashMap;

public class JDBCContext {
   private HashMap data = new HashMap();

   public Object get(Object key) {
      return data.get(key);
   }

   public void put(Object key, Object value) {
      data.put(key, value);
   }

   public void remove(Object key) {
      data.remove(key);
   }
}
