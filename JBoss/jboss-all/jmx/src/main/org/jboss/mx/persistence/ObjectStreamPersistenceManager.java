package org.jboss.mx.persistence;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.Descriptor;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.modelmbean.ModelMBeanAttributeInfo;
import javax.management.modelmbean.ModelMBeanInfo;

import org.jboss.mx.modelmbean.ModelMBeanConstants;
import org.jboss.mx.modelmbean.ModelMBeanInvoker;
import org.jboss.mx.persistence.PersistenceManager;

import org.jboss.logging.Logger;
import org.jboss.util.Strings;

/**
 * Object Stream Persistence Manager. <p>
 *
 * Persists the MBean to the file system using an Object Stream.
 * Includes code based on examples in Juha's JMX Book. <p>
 *
 * Object Streams written to disk are admittedly lacking in the area of
 * "long-term", "portable", or "human-readable" persistence.  They are fairly
 * straightforward, however.
 * Primarily, this class is useful for demonstration and "quick & dirty" persistence.
 *
 * @todo  currently metadata as well as data is stored. only data needs to be stored.
 * @author Matt Munz
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class ObjectStreamPersistenceManager
    extends Object
    implements PersistenceManager
{
   protected static Logger log = Logger.getLogger(ObjectStreamPersistenceManager.class);
   /** A flag set to true to prevent attribute updates from within load
    * triggering stores.
    */
   protected boolean isLoading;

   // Constructors --------------------------------------------------

   public ObjectStreamPersistenceManager()
   {
      super();
   }


   // Public --------------------------------------------------------
   
   /**
    * deserializes state from the object input stream
    *
    * @param  mbean
    * @param  metadata
    * @exception  MBeanException
    */
   public void load(ModelMBeanInvoker mbean, MBeanInfo metadata) throws MBeanException
   {
      log.debug("load, resource:"+mbean.getResource());

      if (metadata == null)
      {
         return;
      }
      if( log.isTraceEnabled() )
         log.trace("metadata: "+metadata);

      File storeFile = getStoreFile(metadata, false);
      if (storeFile == null)
      {
         return;
      }

      try
      {
         FileInputStream fis = new FileInputStream(storeFile);
         ObjectInputStream ois = new ObjectInputStream(fis);
         ModelMBeanInfo storeMetadata = (ModelMBeanInfo) ois.readObject();
         ois.close();
         log.debug("metadata deserialized");
         if( log.isTraceEnabled() )
            log.trace("storeMetadata: "+storeMetadata);
         loadFromMetadata(mbean, storeMetadata);
      }
      catch (Exception e)
      {
         log.error("Error loading MBean state", e);
      }
   }

   /** What we need to get here is 1) the persist location, and 2) the entire
    * contents of the mbean. #2 contains the entire contents (state) of the
    * model object, as well as the meta data that the mbean provides.
    * As such, serializing this (MBeanInfo) object (brute force) in effect
    * serializes the model as well.
    *
    * @param  metadata
    * @exception  MBeanException
    */
   public void store(MBeanInfo metadata) throws MBeanException
   {
      if (isLoading())
      {
         return;
      }

      log.debug("store");
      if( log.isTraceEnabled() )
         log.trace("metadata: " + metadata);
      File storeFile = getStoreFile(metadata, true);
      if( storeFile == null )
      {
         return;
      }

      try
      {
         log.debug("Storing to file: "+storeFile.getAbsolutePath());
         FileOutputStream fos = new FileOutputStream(storeFile);
         ObjectOutputStream oos = new ObjectOutputStream(fos);
         oos.writeObject(metadata);
      }
      catch (IOException e)
      {
         throw new MBeanException(e, "Error in persisting MBean.");
      }
   }

   // Protected -----------------------------------------------------
   
   /** Obtain the attribute values from the metadata and invoke setAttributes
    * on the mbean invoker.
    *
    * @param mbean the invoker and assocaited mbean resource
    * @param metadata the metadata to use as the attributes value source
    */
   protected void loadFromMetadata(ModelMBeanInvoker mbean, ModelMBeanInfo metadata)
   {
      AttributeList attributes = new AttributeList();
      // iterate over all attributes in metadata
      MBeanAttributeInfo[] attrs = metadata.getAttributes();
      for (int i = 0; i < attrs.length; i++)
      {
         /// for each attribute, create a new Attribute object and add it to the collection
         ModelMBeanAttributeInfo attributeInfo = (ModelMBeanAttributeInfo)attrs[i];
         Descriptor attrDesc = attributeInfo.getDescriptor();
         Object name = attrDesc.getFieldValue(ModelMBeanConstants.NAME);
         Object value = attrDesc.getFieldValue(ModelMBeanConstants.VALUE);
         log.debug("loading attribute  name: " + name + ", value: " + value);
         Attribute curAttribute = new Attribute(name.toString(), value);
         attributes.add(curAttribute);
      }

      try
      {
         setIsLoading(true);
         mbean.setAttributes(attributes);
      }
      finally
      {
         setIsLoading(false);
      }
   }

   protected boolean isLoading()
   {
      return isLoading;
   }

   protected void setIsLoading(boolean newIsLoading)
   {
      isLoading = newIsLoading;
   }
   protected File getStoreFile(MBeanInfo metadata, boolean createFile)
      throws MBeanException
   {
      Descriptor d = ((ModelMBeanInfo)metadata).getMBeanDescriptor();
      String dirPath = (String)d.getFieldValue(ModelMBeanConstants.PERSIST_LOCATION);
      String file = (String) d.getFieldValue(ModelMBeanConstants.PERSIST_NAME);
      if( dirPath == null )
      {
         log.debug("No "+ModelMBeanConstants.PERSIST_LOCATION
            +" descriptor value found, using '.'");
         dirPath = ".";
      }
      if( file == null )
      {
         log.debug("No "+ModelMBeanConstants.PERSIST_NAME+" descriptor value found");
         return null;
      }

      dirPath = Strings.replaceProperties(dirPath);
      file = Strings.replaceProperties(file);
      File dir = new File(dirPath);
      File storeFile = new File(dir, file);
      boolean exists = storeFile.exists();
      log.debug("Store file is: "+storeFile.getAbsolutePath());
      if( exists == false && createFile == true )
      {
         dir.mkdirs();
         try
         {
            storeFile.createNewFile();
         }
         catch(IOException e)
         {
            throw new MBeanException(e, "Failed to create store file");
         }
      }
      else if( exists == false )
      {
         storeFile = null;
      }
      return storeFile;
   }
}
