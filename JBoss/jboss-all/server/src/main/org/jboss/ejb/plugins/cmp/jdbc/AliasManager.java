/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc;

import java.util.Map;
import java.util.HashMap;


/**
 * This class manages aliases for generated queries.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1.4.3 $
 */
final class AliasManager
{
   private static final String RELATION_TABLE_SUFFIX = "_RELATION_TABLE";

   private final String prefix;
   private final String suffix;
   private final int maxLength;
   private final Map aliases = new HashMap();
   private final Map relationTableAliases = new HashMap();

   private int count = 0;

   public AliasManager(String prefix, String suffix, int maxLength)
   {
      this.prefix = prefix;
      this.suffix = suffix;
      this.maxLength = maxLength;
   }

   public String getAlias(String path)
   {
      String alias = (String)aliases.get(path);
      if(alias == null)
      {
         alias = createAlias(path);
         aliases.put(path, alias);
      }
      return alias;
   }

   private String createAlias(String path)
   {
      StringBuffer alias = new StringBuffer();
      alias.append(prefix).append(count++).append(suffix);
      alias.append(path.replace('.', '_'));

      return alias.substring(0, Math.min(maxLength, alias.length()));
   }

   public void addAlias(String path, String alias)
   {
      aliases.put(path, alias);
   }

   public String getRelationTableAlias(String path)
   {
      String relationTableAlias = (String)relationTableAliases.get(path);
      if(relationTableAlias == null)
      {
         relationTableAlias = createRelationTableAlias(path);
         relationTableAliases.put(path, relationTableAlias);
      }
      return relationTableAlias;
   }

   private String createRelationTableAlias(String path)
   {
      StringBuffer relationTableAlias = new StringBuffer();

      relationTableAlias.append(prefix).append(count++).append(suffix);

      relationTableAlias.append(path.replace('.', '_'));
      relationTableAlias.append(RELATION_TABLE_SUFFIX);

      return relationTableAlias.substring(
         0, Math.min(maxLength, relationTableAlias.length()));
   }
}
