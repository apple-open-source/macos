/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.modelmbean;

/**
 * Constraint definitions for the {@link XMBean} implementation.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.3.4.1 $
 */
public interface XMBeanConstants extends ModelMBeanConstants
{

   // Resource Types ------------------------------------------------
   
   /**
    * Resource type string identifying a resource object that implements
    * an interface adhering to the Standard MBean naming conventions.
    */
   final static String STANDARD_INTERFACE = "StandardInterface";   
   //final static String EXTENDED_INTERFACE = "ExtendedInterface";
   
   /**
    * Resource type string identifying a resource object that is part of
    * a descriptor object.
    */
   final static String DESCRIPTOR = "descriptor";

   
   // Descriptor string prefixes ------------------------------------   
   
   /**
    * Descriptor field naming prefix for {@link XMBean} configuration. Descriptor
    * fields matching to this prefix follow the <tt>"xmbean.*"</tt> naming 
    * convention in field names.
    */
   final static String XMBEAN_DESCRIPTOR_PREFIX   = "xmbean.";

   /**
    * Descriptor field naming prefix for {@link XMBean} configuration. Descriptor
    * fields matching to this prefix follow the <tt>"xmbean.metadata.*"</tt> naming 
    * convention in field names.
    */
   final static String METADATA_DESCRIPTOR_PREFIX = XMBEAN_DESCRIPTOR_PREFIX + "metadata.";

   /**
    * Descriptor field naming prefix for {@link XMBean} configuration. Descriptor
    * fields matching to this prefix follow the <tt>"xmbean.resource.*"</tt> naming 
    * convention in field names.
    */
   final static String RESOURCE_DESCRIPTOR_PREFIX = XMBEAN_DESCRIPTOR_PREFIX + "resource.";
   
   
   // Resource descriptor string ------------------------------------
   
   /**
   * Mandatory descriptor field when {@link #DESCRIPTOR} resource type is used.
   * The value of this field contains a reference to the resource
   * object the Model MBean represents.   <p>
   *
   * This field matches to the {@link #RESOURCE_DESCRIPTOR_PREFIX} naming pattern.
   *
   * @see org.jboss.mx.modelmbean.XMBean
   */
   final static String RESOURCE_REFERENCE         = RESOURCE_DESCRIPTOR_PREFIX + "reference";
   
   /**
   * Mandatory descriptor field when {@link #DESCRIPTOR} resource type is used.
   * The value of this field contains the actual resource type of the resource
   * object defined by the {@link #RESOURCE_REFERENCE} field.   <p>
   *
   * This field matches to the {@link #RESOURCE_DESCRIPTOR_PREFIX} naming pattern.
   *
   * @see org.jboss.mx.modelmbean.XMBean
   */
   final static String RESOURCE_TYPE              = RESOURCE_DESCRIPTOR_PREFIX + "type";
   
   
   // Metadata configuration descriptor strings --------------------
   
   /**
    * XML metadata descriptor field name. This descriptor field matches the
    * {@link #METADATA_DESCRIPTOR_PREFIX} and is therefore passed as a 
    * configuration property to all metadata builder implementations.   <p>
    *
    * This specific field is used in a {@link #DESCRIPTOR} resource type to
    * configure <a href="http://www.jdom.org">JDOM</a> based metadata builders
    * to override the default JAXP SAX parser settings. 
    *
    * @see  org.jboss.mx.modelmbean.XMBean
    */
   final static String SAX_PARSER               = METADATA_DESCRIPTOR_PREFIX + "sax.parser";
   
   /**
    * XML metadata descriptor field name. This descriptor field matches the
    * {@link #METADATA_DESCRIPTOR_PREFIX} and is therefore passed as a 
    * configuration property to all metadata builder implementations.   <p>
    *
    * This specific field can be used to indicate the XML based builder 
    * implementations to validate the document instance before creating the
    * metadata.
    *
    * @see  org.jboss.mx.modelmbean.XMBean
    */    
   final static String XML_VALIDATION           = METADATA_DESCRIPTOR_PREFIX + "xml.validate";

   

   // Constants used by the 1.0 xmbean parser.
   // These were never actually implemented.
   final static String STATE_ACTION_ON_UPDATE = "state-action-on-update";
   final static String KEEP_RUNNING = "keep-running";
   final static String RESTART = "restart";
   final static String RECONFIGURE = "reconfigure";
   final static String REINSTANTIATE = "reinstantiate";
   final static String[] STATE_ACTION_ON_UPDATE_LIST = {KEEP_RUNNING, RESTART, RECONFIGURE, REINSTANTIATE};
   final static String[] PERSIST_POLICY_LIST = {NEVER, ON_UPDATE, NO_MORE_OFTEN_THAN, ON_TIMER};

   
}
