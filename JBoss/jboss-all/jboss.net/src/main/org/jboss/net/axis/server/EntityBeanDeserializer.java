/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: EntityBeanDeserializer.java,v 1.2.4.2 2002/11/20 04:00:57 starksm Exp $

package org.jboss.net.axis.server;

import org.jboss.net.axis.ParameterizableDeserializer;

import org.xml.sax.Attributes;
import org.xml.sax.SAXException;

import javax.xml.namespace.QName;

import org.apache.axis.encoding.DeserializerImpl;
import org.apache.axis.encoding.DeserializationContext;
import org.apache.axis.encoding.Deserializer;
import org.apache.axis.encoding.ser.SimpleDeserializer;
import org.apache.axis.encoding.Target;
import org.apache.axis.encoding.TypeMapping;
import org.apache.axis.utils.JavaUtils;
import org.apache.axis.utils.Messages;
import org.apache.axis.Constants;
import org.apache.axis.description.TypeDesc;
import org.apache.axis.message.SOAPHandler;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import javax.naming.InitialContext;
import javax.ejb.EJBHome;
import javax.naming.NamingException;

import java.beans.PropertyDescriptor;
import java.beans.IntrospectionException;
import java.beans.Introspector;

import java.util.Map;
import java.util.List;
import java.util.Collection;
import java.util.Iterator;
import java.util.StringTokenizer;

/**
 * Server-side deserializer hitting an existing entity bean. Derived
 * from the axis BeanDeserializer. Currently relies on some
 * silly conventions that must be configurable in the deployment 
 * descriptor.
 * @author jung
 * @created 21.03.2002
 * @version $Revision: 1.2.4.2 $
 */

public class EntityBeanDeserializer
   extends DeserializerImpl
   implements ParameterizableDeserializer {

   //
   // Attributes
   //

   protected Map options;

   protected Object home;
   protected Method findMethod;
   protected List findElements = new java.util.ArrayList(1);
   protected Object[] findObjects;
   protected TypeDesc typeDesc;
   protected QName xmlType;
   protected Class javaType;
   protected Map propertyMap = new java.util.HashMap(4);
   protected int collectionIndex = -1;
   protected Collection fieldSetters = new java.util.ArrayList(4);
   protected boolean initialized = false;

   /** 
    * Construct a new BeanSerializer
    * @param remoteType remote interface of the entity bean
    * @param xmlType fully-qualified xml tag-name of the corresponding xml structure
    */

   public EntityBeanDeserializer(Class remoteType, QName xmlType)
      throws Exception {
      // first the default constructor
      this.xmlType = xmlType;
      this.javaType = remoteType;
   }

   /** returns an option string with a default */
   protected String getStringOption(String key, String def) {
      String value = (String) options.get(key);
      if (value == null) {
         value = def;
      }
      return value;
   }

   /**
    * initialize the deserializer 
    */

   protected void initialize() throws SAXException {
      if (!initialized) {
         initialized = true;

         try {
            //
            // Extract home from jndiName
            //
            this.home =
                new InitialContext().lookup(
                  getStringOption("JndiName", javaType.getName() + "Home"));

            //
            // Extract find method from name and sig
            //

            String findMethodName = getStringOption("FindMethodName", "findByPrimaryKey");
            String findMethodSignatureString =
               getStringOption("FindMethodSignature", "java.lang.String");
            List findMethodSignatureClasses = new java.util.ArrayList(1);
            StringTokenizer tokenizer = new StringTokenizer(findMethodSignatureString, ";");
            while (tokenizer.hasMoreTokens()) {
               findMethodSignatureClasses.add(
                  Thread.currentThread().getContextClassLoader().loadClass(
                     tokenizer.nextToken()));
            }
            this.findMethod =
               home.getClass().getMethod(
                  findMethodName,
                  (Class[]) findMethodSignatureClasses.toArray(
                     new Class[findMethodSignatureClasses.size()]));

            //
            // Do some reasonable preprocessing
            //

            // Get a list of the bean properties
            BeanPropertyDescriptor[] pd = getPd(javaType);
            // loop through properties and grab the names for later
            for (int i = 0; i < pd.length; i++) {
               BeanPropertyDescriptor descriptor = pd[i];
               propertyMap.put(descriptor.getName(), descriptor);
               propertyMap.put(JavaUtils.xmlNameToJava(descriptor.getName()), descriptor);
            }
            typeDesc = TypeDesc.getTypeDescForClass(javaType);

            //
            // Next prepare the elements we need to call the finder
            //

            String findMethodElements = getStringOption("FindMethodElements", "name");
            tokenizer = new StringTokenizer(findMethodElements, ";");
            while (tokenizer.hasMoreElements()) {
               if (typeDesc != null) {
                  this.findElements.add(typeDesc.getAttributeNameForField(tokenizer.nextToken()));
               } else {
                  this.findElements.add(new QName("", tokenizer.nextToken()));
               }
            }

            this.findObjects = new Object[findElements.size()];
         } catch (NamingException e) {
            throw new SAXException("Could not lookup home.", e);
         } catch (ClassNotFoundException e) {
            throw new SAXException("Could not find signature class.", e);
         } catch (NoSuchMethodException e) {
            throw new SAXException("Could not find finder method.", e);
         }

      }
   }

   public void setOptions(Map options) {
      this.options = options;
   }

   public Map getOptions() {
      return options;
   }

   /**
    * Deserializer interface called on each child element encountered in
    * the XML stream.
    * @param namespace is the namespace of the child element
    * @param localName is the local name of the child element
    * @param prefix is the prefix used on the name of the child element
    * @param attributes are the attributes of the child element
    * @param context is the deserialization context.
    * @return is a Deserializer to use to deserialize a child (must be
    * a derived class of SOAPHandler) or null if no deserialization should
    * be performed.
    */
   public SOAPHandler onStartChild(
      String namespace,
      String localName,
      String prefix,
      Attributes attributes,
      DeserializationContext context)
      throws SAXException {
      BeanPropertyDescriptor propDesc = null;

      if (typeDesc != null) {
         QName elemQName = new QName(namespace, localName);
         String fieldName = typeDesc.getFieldNameForElement(elemQName,false);
         propDesc = (BeanPropertyDescriptor) propertyMap.get(fieldName);
      }

      if (propDesc == null) {
         // look for a field by this name.
         propDesc = (BeanPropertyDescriptor) propertyMap.get(localName);
      }
      if (propDesc == null) {
         // look for a field by the "adjusted" name.
         propDesc =
            (BeanPropertyDescriptor) propertyMap.get(JavaUtils.xmlNameToJava(localName));
      }

      if (propDesc == null) {
         // No such field
         throw new SAXException(
            Messages.getMessage("badElem00", javaType.getName(), localName));
      }

      // Determine the QName for this child element.
      // Look at the type attribute specified.  If this fails,
      // use the javaType of the property to get the type qname.
      QName qn = context.getTypeFromAttributes(namespace, localName, attributes);

      // get the deserializer
      Deserializer dSer = context.getDeserializerForType(qn);

      // If no deserializer, use the base DeserializerImpl.
      // There may not be enough information yet to choose the
      // specific deserializer.
      if (dSer == null) {
         dSer = new DeserializerImpl();
         // determine a default type for this child element
         TypeMapping tm = context.getTypeMapping();
         Class type = propDesc.getType();
         dSer.setDefaultType(tm.getTypeQName(type));
      }

      QName elementQName = new QName(namespace, localName);
      if (findElements.contains(elementQName)) {
         dSer.registerValueTarget(
            new FindPropertyTarget(findElements.indexOf(elementQName)));
      } else if (propDesc.getWriteMethod().getParameterTypes().length == 1) {
         // Success!  Register the target and deserializer.
         collectionIndex = -1;
         dSer.registerValueTarget(new BeanPropertyTarget(propDesc));
      } else {
         // Success! This is a collection of properties so use the index
         collectionIndex++;
         dSer.registerValueTarget(new BeanPropertyTarget(propDesc, collectionIndex));
      }
      return (SOAPHandler) dSer;
   }

   /**
    * Set the bean properties that correspond to element attributes.
    * 
    * This method is invoked after startElement when the element requires
    * deserialization (i.e. the element is not an href and the value is not nil.)
    * @param namespace is the namespace of the element
    * @param localName is the name of the element
    * @param qName is the prefixed qName of the element
    * @param attributes are the attributes on the element...used to get the type
    * @param context is the DeserializationContext
    */
   public void onStartElement(
      String namespace,
      String localName,
      String qName,
      Attributes attributes,
      DeserializationContext context)
      throws SAXException {

      initialize();

      if (typeDesc == null)
         return;

      // loop through the attributes and set bean properties that 
      // correspond to attributes
      for (int i = 0; i < attributes.getLength(); i++) {
         QName attrQName = new QName(attributes.getURI(i), attributes.getLocalName(i));
         String fieldName = typeDesc.getFieldNameForAttribute(attrQName);
         if (fieldName == null)
            continue;

         String attrName = attributes.getLocalName(i);

         // look for the attribute property
         BeanPropertyDescriptor bpd =
            (BeanPropertyDescriptor) propertyMap.get(fieldName);
         if (bpd != null) {
            if (bpd.getWriteMethod() == null)
               continue;

            // determine the QName for this child element
            TypeMapping tm = context.getTypeMapping();
            Class type = bpd.getType();
            QName qn = tm.getTypeQName(type);
            if (qn == null)
               throw new SAXException(Messages.getMessage("unregistered00", type.toString()));

            // get the deserializer
            Deserializer dSer = context.getDeserializerForType(qn);
            if (dSer == null)
               throw new SAXException(Messages.getMessage("noDeser00", type.toString()));
            if (!(dSer instanceof SimpleDeserializer))
               throw new SAXException(
                  Messages.getMessage("AttrNotSimpleType00", bpd.getName(), type.toString()));

            if (findElements.contains(attrQName)) {
               dSer.registerValueTarget(
                  new FindPropertyTarget(findElements.indexOf(attrQName)));
            } else if (bpd.getWriteMethod().getParameterTypes().length == 1) {
               // Success!  Create an object from the string and set
               // it in the bean
               try {
                  Object val = ((SimpleDeserializer) dSer).makeValue(attributes.getValue(i));
                  bpd.getWriteMethod().invoke(value, new Object[] { val });
               } catch (Exception e) {
                  throw new SAXException(e);
               }
            }

         } // if
      } // attribute loop
   }

   public void onEndElement(
      String namespace,
      String localName,
      DeserializationContext context)
      throws SAXException {
      try {
         value = findMethod.invoke(home, findObjects);
         Iterator allSetters = fieldSetters.iterator();
         while (allSetters.hasNext()) {
            ((BeanPropertyTarget) allSetters.next()).setReal(value);
         }
         fieldSetters = null;
      } catch (InvocationTargetException e) {
         throw new SAXException("Encountered exception " + e.getTargetException());
      } catch (IllegalAccessException e) {
         throw new SAXException("Encountered exception " + e);
      }
      super.onEndElement(namespace, localName, context);
   }

   public class FindPropertyTarget implements Target {
      int position;

      public FindPropertyTarget(int index) {
         this.position = index;
      }

      public void set(Object value) throws SAXException {
         findObjects[position] = value;
      }
   }

   /**
    * Class which knows how to update a bean property
    */
   public class BeanPropertyTarget implements Target {
      private BeanPropertyDescriptor pd;
      private int index = -1;
      Object value;

      /** 
       * This constructor is used for a normal property.
       * @param Object is the bean class
       * @param pd is the property
       **/
      public BeanPropertyTarget(BeanPropertyDescriptor pd) {
         this.pd = pd;
         this.index = -1; // disable indexing
      }

      /** 
       * This constructor is used for an indexed property.
       * @param Object is the bean class
       * @param pd is the property
       * @param i is the index          
       **/
      public BeanPropertyTarget(BeanPropertyDescriptor pd, int i) {
         this.pd = pd;
         this.index = i;
      }

      public void set(Object value) throws SAXException {
         this.value = value;
         if (fieldSetters != null) {
            fieldSetters.add(this);
         } else {
            setReal(EntityBeanDeserializer.this.value);
         }
      }

      public void setReal(Object target) throws SAXException {
         try {
            if (index < 0)
               pd.getWriteMethod().invoke(target, new Object[] { value });
            else
               pd.getWriteMethod().invoke(target, new Object[] { new Integer(index), value });
         } catch (Exception e) {
            Class type = pd.getReadMethod().getReturnType();
            value = JavaUtils.convert(value, type);
            try {
               if (index < 0)
                  pd.getWriteMethod().invoke(target, new Object[] { value });
               else
                  pd.getWriteMethod().invoke(target, new Object[] { new Integer(index), value });
            } catch (Exception ex) {
               throw new SAXException(ex);
            }
         }
      }
   }

   static class BeanPropertyDescriptor {
      private String name;
      private Method getter;
      private Method setter;

      public BeanPropertyDescriptor(String _name, Method _getter, Method _setter) {
         name = _name;
         getter = _getter;
         setter = _setter;
      }

      public Method getReadMethod() {
         return getter;
      }
      public Method getWriteMethod() {
         return setter;
      }
      public String getName() {
         return name;
      }
      public Class getType() {
         return getter.getReturnType();
      }

      /** 
       * This method attempts to sort the property descriptors to match the 
       * order defined in the class.  This is necessary to support 
       * xsd:sequence processing, which means that the serialized order of 
       * properties must match the xml element order.  (This method assumes that the
       * order of the set methods matches the xml element order...the emitter 
       * will always order the set methods according to the xml order.)
       *
       * This routine also looks for set(i, type) and get(i) methods and adjusts the 
       * property to use these methods instead.  These methods are generated by the
       * emitter for "collection" of properties (i.e. maxOccurs="unbounded" on an element).
       * JAX-RPC is silent on this issue, but web services depend on this kind of behaviour.
       * The method signatures were chosen to match bean indexed properties.
       */
      static BeanPropertyDescriptor[] processPropertyDescriptors(
         PropertyDescriptor[] rawPd,
         Class cls) {
         BeanPropertyDescriptor[] myPd = new BeanPropertyDescriptor[rawPd.length];

         for (int i = 0; i < rawPd.length; i++) {
            myPd[i] =
               new BeanPropertyDescriptor(
                  rawPd[i].getName(),
                  rawPd[i].getReadMethod(),
                  rawPd[i].getWriteMethod());
         }

         try {
            // Create a new pd array and index into the array
            int index = 0;

            // Build a new pd array
            // defined by the order of the get methods. 
            BeanPropertyDescriptor[] newPd = new BeanPropertyDescriptor[rawPd.length];
            Method[] methods = cls.getMethods();
            for (int i = 0; i < methods.length; i++) {
               Method method = methods[i];
               if (method.getName().startsWith("set")) {
                  boolean found = false;
                  for (int j = 0; j < myPd.length && !found; j++) {
                     if (myPd[j].getWriteMethod() != null
                        && myPd[j].getWriteMethod().equals(method)) {
                        found = true;
                        newPd[index] = myPd[j];
                        index++;
                     }
                  }
               }
            }
            // Now if there are any additional property descriptors, add them to the end.
            if (index < myPd.length) {
               for (int m = 0; m < myPd.length && index < myPd.length; m++) {
                  boolean found = false;
                  for (int n = 0; n < index && !found; n++) {
                     found = (myPd[m] == newPd[n]);
                  }
                  if (!found) {
                     newPd[index] = myPd[m];
                     index++;
                  }
               }
            }
            // If newPd has same number of elements as myPd, use newPd.
            if (index == myPd.length) {
               myPd = newPd;
            }

            // Get the methods of the class and look for the special set and
            // get methods for property "collections"
            for (int i = 0; i < methods.length; i++) {
               if (methods[i].getName().startsWith("set")
                  && methods[i].getParameterTypes().length == 2) {
                  for (int j = 0; j < methods.length; j++) {
                     if ((methods[j].getName().startsWith("get")
                        || methods[j].getName().startsWith("is"))
                        && methods[j].getParameterTypes().length == 1
                        && methods[j].getReturnType() == methods[i].getParameterTypes()[1]
                        && methods[j].getParameterTypes()[0] == int.class
                        && methods[i].getParameterTypes()[0] == int.class) {
                        for (int k = 0; k < myPd.length; k++) {
                           if (myPd[k].getReadMethod() != null
                              && myPd[k].getWriteMethod() != null
                              && myPd[k].getReadMethod().getName().equals(methods[j].getName())
                              && myPd[k].getWriteMethod().getName().equals(methods[i].getName())) {
                              myPd[k] = new BeanPropertyDescriptor(myPd[k].getName(), methods[j], methods[i]);
                           }
                        }
                     }
                  }
               }
            }
         } catch (Exception e) {
            // Don't process Property Descriptors if problems occur
            return myPd;
         }
         return myPd;
      }
   }

   /**
    * Create a BeanPropertyDescriptor array for the indicated class.
    */
   public static BeanPropertyDescriptor[] getPd(Class javaType) {
      BeanPropertyDescriptor[] pd;
      try {
         PropertyDescriptor[] rawPd =
            Introspector.getBeanInfo(javaType).getPropertyDescriptors();
         pd = BeanPropertyDescriptor.processPropertyDescriptors(rawPd, javaType);
      } catch (Exception e) {
         // this should never happen
         throw new RuntimeException(e.getMessage());
      }
      return pd;
   }

}