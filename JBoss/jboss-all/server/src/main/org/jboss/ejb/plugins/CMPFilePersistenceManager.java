/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins;

import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedInputStream;
import java.io.IOException;

import java.lang.reflect.Method;
import java.lang.reflect.Field;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Iterator;

import javax.ejb.EJBObject;
import javax.ejb.Handle;
import javax.ejb.CreateException;
import javax.ejb.DuplicateKeyException;
import javax.ejb.EJBException;
import javax.ejb.FinderException;
import javax.ejb.RemoveException;

import org.jboss.ejb.Container;
import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.EntityPersistenceStore;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.metadata.EntityMetaData;

import org.jboss.system.server.ServerConfigLocator;
import org.jboss.system.ServiceMBeanSupport;

import org.jboss.util.file.FilenameSuffixFilter;

/**
 * A file-based CMP entity bean persistence manager.
 *
 * <p>
 * Reads and writes entity bean objects to files by using the
 * standard Java serialization mechanism.
 * 
 * <p>
 * Enitiy state files are stored under:
 * <tt><em>jboss-server-data-dir</em>/<em>storeDirectoryName</em>/<em>ejb-name</em></tt>.
 *
 * <p>
 * Note, currently the name of the entity must be unique across the server, or
 * unless the store directory is changed, to avoid data collisions.
 * 
 * <p>
 * jason: disabled because XDoclet can not handle \u0000 right now
 * _@_jmx:mbean extends="org.jboss.system.ServiceMBean"
 * 
 * @version <tt>$Revision: 1.19.2.3 $</tt>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * <p><b>20010801 marc fleury:</b>
 * <ul>
 * <li>- insertion in cache upon create in now done in the instance interceptor
 * </ul>
 * <p><b>20011201 Dain Sundstrom:</b>
 * <ul>
 * <li>- added createBeanInstance and initEntity methods
 * </ul>
 * <p><b>20020525 Dain Sundstrom:</b>
 * <ul>
 * <li>- Replaced FinderResults with Collection
 * <li>- Removed unused method loadEntities
 * </ul>
 */
public class CMPFilePersistenceManager
   extends ServiceMBeanSupport
   implements EntityPersistenceStore /*, CMPFilePersistenceManagerMBean */
{
   /** The default store directory name ("<tt>entities</tt>"). */
   public static final String DEFAULT_STORE_DIRECTORY_NAME = "entities";
   
   /** Our container. */
   private EntityContainer con;

   /**
    * The sub-directory name under the server data directory where
    * entity data is stored.
    *
    * @see #DEFAULT_STORE_DIRECTORY_NAME
    * @see setStoreDirectoryName
    */
   private String storeDirName = DEFAULT_STORE_DIRECTORY_NAME;
   
   /** The base directory where bean state will be stored. */
   private File storeDir;

   /** A reference to the ID field for the entiy bean. */
   private Field idField;

   /** Optional isModified method used by storeEntity. */
   private Method isModified;
   
   /**
    * Saves a reference to the {@link EntityContainer} for
    * its bean type.
    *
    * @throws ClassCastException  Container is not a EntityContainer.
    */
   public void setContainer(final Container c)
   {
      con = (EntityContainer)c;
   }

   //
   // jason: these properties are intended to be used when plugins/interceptors 
   //        can take configuration values (need to update xml schema and processors).
   //
   
   /**
    * Set the sub-directory name under the server data directory
    * where entity data will be stored.
    *
    * <p>
    * This value will be appened to the value of
    * <tt><em>jboss-server-data-dir</em></tt>.
    *
    * <p>
    * This value is only used during creation and will not dynamically
    * change the store directory when set after the create step has finished.
    *
    * @jmx:managed-attribute
    *
    * @param dirName   A sub-directory name.
    */
   public void setStoreDirectoryName(final String dirName)
   {
      this.storeDirName = dirName;
   }

   /**
    * Get the sub-directory name under the server data directory
    * where entity data is stored.
    *
    * @jmx:managed-attibute
    *
    * @see #setStoreDirectoryName
    *
    * @return A sub-directory name.
    */
   public String getStoreDirectoryName()
   {
      return storeDirName;
   }

   /**
    * Returns the directory used to store entity state files.
    *
    * @jmx:managed-attibute
    * 
    * @return The directory used to store entity state files.
    */
   public File getStoreDirectory()
   {
      return storeDir;
   }
   
   protected void createService() throws Exception
   {
      boolean debug = log.isDebugEnabled();

      // Initialize the dataStore

      String ejbName = con.getBeanMetaData().getEjbName();

      // Get the system data directory
      File dir = ServerConfigLocator.locate().getServerDataDir();

      //
      // jason: may have to use a generated token from container config
      //        to determine a unique name for this config for the given
      //        entity name.  it must persist through restarts though...
      //

      // Setup the reference to the entity data store directory
      dir = new File(dir, storeDirName);
      dir = new File(dir, ejbName);
      storeDir = dir;
      
      if (debug) {
         log.debug("Storing entity state for '" + ejbName + "' in: " + storeDir);
      }

      // if the directory does not exist then try to create it
      if (!storeDir.exists()) {
         if (!storeDir.mkdirs()) {
            throw new IOException("Failed to create directory: " + storeDir);
         }
      }
      
      // make sure we have a directory
      if (!storeDir.isDirectory()) {
         throw new IOException("File exists where directory expected: " + storeDir);
      }

      // make sure we can read and write to it
      if (!storeDir.canWrite() || !storeDir.canRead()) {
         throw new IOException("Directory must be readable and writable: " + storeDir);
      }

      // Get the ID field
      idField = con.getBeanClass().getField("id");
      if (debug) log.debug("Using id field: " + idField);

      // Lookup the isModified method if it exists
      try
      {
         isModified = con.getBeanClass().getMethod("isModified", new Class[0]);
         if (!isModified.getReturnType().equals(Boolean.TYPE)) {
            isModified = null; // Has to have "boolean" as return type!
            log.warn("Found isModified method, but return type is not boolean; ignoring");
         }
         else {
            if (debug) log.debug("Using isModified method: " + isModified);
         }
      }
      catch (NoSuchMethodException ignored) {}
   }

   /**
    * Try to remove the store directory, if we can't then ignore.
    */
   protected void destroyService() throws Exception
   {
      storeDir.delete();
   }
   
   public Object createBeanClassInstance() throws Exception
   {
      return con.getBeanClass().newInstance();
   }

   /**
    * Reset all attributes to default value
    *
    * <p>
    * The EJB 1.1 specification is not entirely clear about this,
    * the EJB 2.0 spec is, see page 169.
    * Robustness is more important than raw speed for most server
    * applications, and not resetting atrribute values result in
    * *very* weird errors (old states re-appear in different instances and the
    * developer thinks he's on drugs).
    */
   public void initEntity(final EntityEnterpriseContext ctx)
   {
      // first get cmp metadata of this entity
      Object instance = ctx.getInstance();
      Class ejbClass = instance.getClass();
      Field cmpField;
      Class cmpFieldType;

      EntityMetaData metaData = (EntityMetaData)con.getBeanMetaData();
      Iterator i = metaData.getCMPFields();

      while (i.hasNext())
      {
         // get the field declaration
         try
         {
            cmpField = ejbClass.getField((String)i.next());
            cmpFieldType = cmpField.getType();
            // find the type of the field and reset it
            // to the default value
            if (cmpFieldType.equals(boolean.class))
            {
               cmpField.setBoolean(instance,false);
            }
            else if (cmpFieldType.equals(byte.class))
            {
               cmpField.setByte(instance,(byte)0);
            }
            else if (cmpFieldType.equals(int.class))
            {
               cmpField.setInt(instance,0);
            }
            else if (cmpFieldType.equals(long.class))
            {
               cmpField.setLong(instance,0L);
            }
            else if (cmpFieldType.equals(short.class))
            {
               cmpField.setShort(instance,(short)0);
            }
            else if (cmpFieldType.equals(char.class))
            {
               cmpField.setChar(instance,'\u0000');
            }
            else if (cmpFieldType.equals(double.class))
            {
               cmpField.setDouble(instance,0d);
            }
            else if (cmpFieldType.equals(float.class))
            {
               cmpField.setFloat(instance,0f);
            }
            else
            {
               cmpField.set(instance,null);
            }
         }
         catch (NoSuchFieldException e)
         {
            // will be here with dependant value object's private attributes
            // should not be a problem
         }
         catch (Exception e)
         {
            throw new EJBException(e);
         }
      }
   }

   public Object createEntity(final Method m,
                              final Object[] args,
                              final EntityEnterpriseContext ctx)
      throws Exception
   {                      
      try { 
         Object id = idField.get(ctx.getInstance());
         
         // Check exist
         if (getFile(id).exists())
            throw new DuplicateKeyException("Already exists: "+id);
         
         // Store to file
         storeEntity(id, ctx.getInstance());
         
         return id;
      } 
      catch (IllegalAccessException e)
      {
         throw new CreateException("Could not create entity: "+e);
      }
   }

   public Object postCreateEntity(final Method m,
                                  final Object[] args,
                                  final EntityEnterpriseContext ctx)
      throws Exception
   {
      return null;
   }

   public Object findEntity(final Method finderMethod,
                            final Object[] args,
                            final EntityEnterpriseContext ctx)
      throws FinderException
   {
      if (finderMethod.getName().equals("findByPrimaryKey"))
      {
         if (!getFile(args[0]).exists())
            throw new FinderException(args[0]+" does not exist");
            
         return args[0];
      }

      return null;
   }
     
   public Collection findEntities(final Method finderMethod,
                                  final Object[] args,
                                  final EntityEnterpriseContext ctx)
   {
      if (finderMethod.getName().equals("findAll"))
      {
         String[] files = storeDir.list(new FilenameSuffixFilter(".ser"));
         ArrayList result = new ArrayList(files.length);
         for (int i = 0; i < files.length; i++) {
            result.add(files[i].substring(0,files[i].length()-4));
         }
         
         return result;
      }
      else
      {
         // we only support find all
         return Collections.EMPTY_LIST;
      }
   }

   /**
    * Non-operation.
    */
   public void activateEntity(final EntityEnterpriseContext ctx)
   {
      // Nothing to do
   }
   
   public void loadEntity(final EntityEnterpriseContext ctx)
   {
      try
      {
         Object obj = ctx.getInstance();
         
         // Read fields
         ObjectInputStream in = new CMPObjectInputStream
            (new BufferedInputStream(new FileInputStream(getFile(ctx.getId()))));

         try {
            Field[] f = obj.getClass().getFields();
            for (int i = 0; i < f.length; i++)
            {
               f[i].set(obj, in.readObject());
            }
         }
         finally {
            in.close();
         }
      }
      catch (Exception e)
      {
         throw new EJBException("Load failed", e);
      }
   }
      
   private void storeEntity(Object id, Object obj) 
   {
      try
      {
         // Store fields
         ObjectOutputStream out = new CMPObjectOutputStream
            (new BufferedOutputStream(new FileOutputStream(getFile(id))));
         
         try {
            Field[] f = obj.getClass().getFields();
            for (int i = 0; i < f.length; i++)
            {
               out.writeObject(f[i].get(obj));
            }
         }
         finally {
            out.close();
         }
      }
      catch (Exception e)
      {
         throw new EJBException("Store failed", e);
      }
   }

   public boolean isModified(final EntityEnterpriseContext ctx) throws Exception 
   {
      if (isModified == null)
      {
         return true;
      }

      Boolean modified = (Boolean) isModified.invoke(ctx.getInstance(), new Object[0]);
      return modified.booleanValue();
   }
  
   public void storeEntity(final EntityEnterpriseContext ctx)
   {
      storeEntity(ctx.getId(), ctx.getInstance());
   }

   /**
    * Non-operation.
    */
   public void passivateEntity(final EntityEnterpriseContext ctx)
   {
      // This plugin doesn't do anything specific
   }
      
   public void removeEntity(final EntityEnterpriseContext ctx)
      throws RemoveException
   {
      // Remove file
      File file = getFile(ctx.getId());
      
      if (!file.delete()) {
         throw new RemoveException("Could not remove file: " + file);
      }
   }
   
   protected File getFile(final Object id)
   {
      return new File(storeDir, String.valueOf(id) + ".ser");
   }
    
   // Inner classes -------------------------------------------------
   
   static class CMPObjectOutputStream
      extends ObjectOutputStream
   {
      public CMPObjectOutputStream(final OutputStream out)
         throws IOException
      {
         super(out);
         enableReplaceObject(true);
      }
      
      protected Object replaceObject(final Object obj)
         throws IOException
      {
         if (obj instanceof EJBObject)
            return ((EJBObject)obj).getHandle();
            
         return obj;
      }
   }
   
   static class CMPObjectInputStream
      extends ObjectInputStream
   {
      public CMPObjectInputStream(final InputStream in)
         throws IOException
      {
         super(in);
         enableResolveObject(true);
      }
      
      protected Object resolveObject(final Object obj)
         throws IOException
      {
         if (obj instanceof Handle)
            return ((Handle)obj).getEJBObject();
            
         return obj;
      }
   }
}

