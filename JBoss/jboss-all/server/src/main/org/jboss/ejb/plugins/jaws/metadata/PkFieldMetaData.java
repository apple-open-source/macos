/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.jaws.metadata;

import java.lang.reflect.Field;

/**
 * This is a wrapper class that holds all the
 * information JawsPersistenceManager commands
 * need about a primary key field.
 * 
 * This class was org.jboss.ejb.plugins.PkFieldInfo
 *   
 * @see <related>
 * @author <a href="mailto:justin@j-m-f.demon.co.uk">Justin Forder</a>
 * @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @version $Revision: 1.4 $
 */

public class PkFieldMetaData
{
   // Attributes ----------------------------------------------------
    
   private Field pkField = null;

   private CMPFieldMetaData cmpField;
   
   private JawsEntityMetaData jawsEntity;
   
   
   
   // Constructors --------------------------------------------------
   
   // This constructor is used when the primary key is composite.
   // The pkField is a field on the primary key class.
   public PkFieldMetaData(Field pkField, CMPFieldMetaData cmpField, JawsEntityMetaData jawsEntity)
   {
      this(cmpField, jawsEntity);
      this.pkField = pkField;
   }
   
   // This constructor is used when the primary key is simple.
   // There is no primary key class, and there is no pkField.
   public PkFieldMetaData(CMPFieldMetaData cmpField, JawsEntityMetaData jawsEntity)
   {
      this.cmpField = cmpField;
	  this.jawsEntity = jawsEntity;
   }
   
   
   // Public --------------------------------------------------------
   
   public final String getName()
   {
      return cmpField.getName();
   }
   
   // N.B. This returns null if the primary key is not composite.
   public final Field getPkField()
   {
      return pkField;
   }
   
   public final Field getCMPField()
   {
      return cmpField.getField();
   }
   
   public final int getJDBCType()
   {
      return cmpField.getJDBCType();
   }
   
   public final String getSQLType()
   {
      return cmpField.getSQLType();
   }
   
   public final String getColumnName()
   {
      return cmpField.getColumnName();
   }
   
   public JawsEntityMetaData getJawsEntity() {
      return jawsEntity;
   }
}
