/*O
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.modelmbean;

/**
 * Constants used with Model MBean implementations.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author Matt Munz
 */
public interface ModelMBeanConstants
{
   // Model MBean notification type string -------------------------
   final static String GENERIC_MODELMBEAN_NOTIFICATION = "jmx.modelmbean.generic";
   
   
   // Model MBean resource types -----------------------------------
   
   /**
    * Default Model MBean resource type, <tt>"ObjectReference"</tt>.
    */
   final static String OBJECT_REF               = "ObjectReference";

   
   // Mandatory descriptor fields -----------------------------------
   
   /**
    * Descriptor field string, <tt>"name"</tt>. Note that all valid descriptors
    * should contain this field.
    */
   final static String NAME                     = "name";
   
   /**
    * Descriptor field string, <tt>"descriptorType"</tt>. Note that all valid
    * descriptors should contain this field.
    */
   final static String DESCRIPTOR_TYPE          = "descriptorType";
   
   /**
    * Descriptor field string, <tt>"role"</tt>.
    */
   final static String ROLE                     = "role";
   
   
   // Descriptor types ----------------------------------------------
   
   /**
    * MBean descriptor type string, <tt>"MBean"</tt>.
    */
   final static String MBEAN_DESCRIPTOR         = "MBean";
   
   /**
    * MBean attribute descriptor type string, <tt>"attribute"</tt>.
    */
   final static String ATTRIBUTE_DESCRIPTOR     = "attribute";
   
   /**
    * MBean operation descriptor type string, <tt>"operation"</tt>.
    */
   final static String OPERATION_DESCRIPTOR     = "operation";
   
   /**
    * MBean notification descriptor type string, <tt>"notification"</tt>.
    */
   final static String NOTIFICATION_DESCRIPTOR  = "notification";
   
   /**
    * MBean constructor descriptor type string, <tt>"operation"</tt>.<br>
    * NOTE: the <tt>role</tt> field will contain the value <tt>"constructor"</tt>
    * (see {@link #CONSTRUCTOR}).
    */
   final static String CONSTRUCTOR_DESCRIPTOR   = "constructor";
      
   /**
    * A convenience constant to use with 
    * {@link javax.management.modelmbean.ModelMBeanInfo#getDescriptors getDescriptors()}
    * to return the descriptors of all management interface elements 
    * (a <tt>null</tt> string).
    */
   final static String ALL_DESCRIPTORS          = null;
   
   
   // Operation roles -----------------------------------------------
   
   /**
    * Management operation role string, <tt>"getter"</tt>.
    */
   final static String GETTER                   = "getter";
   
   /**
    * Management operation role string, <tt>"setter"</tt>.
    */
   final static String SETTER                   = "setter";
   
   /**
    * Management operation role string, <tt>"constructor"</tt>.
    */
   final static String CONSTRUCTOR              = "constructor";
   
   
   // Optional descriptor fields ------------------------------------
   final static String VISIBILITY               = "visibility";
   final static String LOG                      = "log";
   final static String EXPORT                   = "export";
   final static String DISPLAY_NAME             = "displayName";
   final static String DEFAULT                  = "default";
   final static String VALUE                    = "value";
   final static String GET_METHOD               = "getMethod";
   final static String SET_METHOD               = "setMethod";
   final static String PERSIST_POLICY           = "persistPolicy";
   final static String PERSIST_PERIOD           = "persistPeriod";
   final static String PERSIST_NAME             = "persistName";
   final static String PERSIST_LOCATION         = "persistLocation";
   final static String CURRENCY_TIME_LIMIT      = "currencyTimeLimit";
   final static String LAST_UPDATED_TIME_STAMP  = "lastUpdatedTimeStamp";
   /** */
   final static String INTERCEPTORS = "interceptors";

   // constant used by the 1.0 xmbean parser
   // this defines the name of the descriptor used to designate the persistence manager 
   // that is to be used for a given XMBean
   final static String PERSISTENCE_MANAGER = "persistence-manager";
   
   
   // Visibility values ---------------------------------------------
   final static String HIGH_VISIBILITY          = "1";
   final static String NORMAL_VISIBILITY        = "2";
   final static String LOW_VISIBILITY           = "3";
   final static String MINIMAL_VISIBILITY       = "4";
   
   
   // Persistence policies ------------------------------------------
   final static String ON_UPDATE                = "OnUpdate";
   final static String NO_MORE_OFTEN_THAN       = "NoMoreOftenThan";
   final static String NEVER                    = "Never";
   final static String ON_TIMER                 = "OnTimer";
         
         
   // Constants for metadata objects --------------------------------
   final static boolean IS_READABLE             = true;
   final static boolean IS_WRITABLE             = true;
   final static boolean IS_IS                   = true;

   
   // Operation impact ----------------------------------------------
   final static String ACTION                   = "ACTION";
   final static String ACTION_INFO              = "ACTION_INFO";
   final static String INFO                     = "INFO";
   
}

