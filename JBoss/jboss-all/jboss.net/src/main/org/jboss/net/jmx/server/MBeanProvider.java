/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: MBeanProvider.java,v 1.7.4.4 2003/06/16 20:50:39 ejort Exp $

package org.jboss.net.jmx.server;

// axis package
import org.apache.axis.AxisFault;
import org.apache.axis.Message;
import org.apache.axis.MessageContext;
import org.apache.axis.providers.BasicProvider;
import org.apache.axis.providers.java.JavaProvider;
import org.apache.axis.handlers.soap.SOAPService;
import org.apache.axis.message.SOAPEnvelope;
import org.apache.axis.message.RPCParam;
import org.apache.axis.message.RPCElement;
import org.apache.axis.utils.JavaUtils;
import org.apache.axis.utils.Messages;
import org.apache.axis.EngineConfiguration;
import org.apache.axis.wsdl.fromJava.Emitter;
import org.apache.axis.wsdl.fromJava.Types;
import org.apache.axis.description.OperationDesc;
import org.apache.axis.description.ParameterDesc;
import org.apache.axis.description.ServiceDesc;
import org.apache.axis.encoding.TypeMapping;

// Jboss
import org.jboss.net.axis.XMLResourceProvider;
import org.jboss.mx.util.MBeanServerLocator;

// sax & jaxrpc
import org.xml.sax.SAXException;
import javax.wsdl.Definition;
import javax.wsdl.factory.WSDLFactory;
import javax.xml.soap.SOAPException;
import javax.xml.namespace.QName;

// jmx
import javax.management.MBeanParameterInfo;
import javax.management.MBeanServer;
import javax.management.MBeanException;
import javax.management.ReflectionException;
import javax.management.InstanceNotFoundException;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.Attribute;
import javax.management.MBeanInfo;
import javax.management.MBeanAttributeInfo;
import javax.management.AttributeNotFoundException;
import javax.management.InvalidAttributeValueException;
import javax.management.IntrospectionException;
import javax.management.MBeanOperationInfo;

// W3C
import org.w3c.dom.Document;

// utils
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * Exposes mbeans as targets (pivot-handlers) of web-services. To
 * deploy a particular mbean as a web-service, a deployment descriptor
 * would look like:
 *
 * <wsdd:deployment>
 *  <handler name="MBeanDispatcher" class="org.jboss.net.jmx.MBeanProvider"/>
 *  <wsdd:service name="${ServiceName}" handler="Handler">
 *      <option name="handlerClass" value="org.jboss.net.jmx.server.MBeanProvider"/>
 *      <option name="ObjectName" value="${JMX_ObjectName}"/>
 *      <option name="allowedMethods" value="method1 method2 method3"/>
 *      <option name="allowedWriteAttributes" value="attr1 attr2 attr3"/>
 *      <option name="allowedReadAttributes" value="attr4 attr5 attr6"/>
 *  </wsdd:service>
 * </wsdd:deployment>
 * <p>
 * MBeanProvider is able to recognize an {@link WsdlAwareHttpActionHandler} in its
 * transport chain such that it will set the soap-action headers in the wsdl.
 * </p>
 * @created 1. Oktober 2001, 16:38
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.7.4.4 $
 */

public class MBeanProvider extends BasicProvider {

   //
   // Attributes
   //

   /** the server which we are tight to */
   protected MBeanServer server;
   /** the objectName which we are running against */
   protected ObjectName name;
   /** stores meta-data about mbean */
   protected Map attributeData = new java.util.HashMap();
   protected Map methodData = new java.util.HashMap();
   /** which methods are allowed to be exposed */
   protected String allowedMethodsOption = "allowedMethods";
   /** which attributes are allowed to be exposed */
   protected String allowedReadAttributesOption = "allowedReadAttributes";
   /** which attributes are allowed to be exposed */
   protected String allowedWriteAttributesOption = "allowedWriteAttributes";

   //
   // Constructors
   //

   /**
    * Constructor MBeanProvider
    */
   public MBeanProvider() {
   }

   //
   // Public API
   //

   /* 
    * initialize the meta-data 
    * @see org.apache.axis.providers.BasicProvider#initServiceDesc(SOAPService, MessageContext)
    */
   public void initServiceDesc(SOAPService service, MessageContext msgCtx)
      throws AxisFault {
      // we may be called without context when html-gui is hit
      EngineConfiguration engineConfig =
         msgCtx != null ? msgCtx.getAxisEngine().getConfig() : null;
      ClassLoader currentLoader =
         Thread.currentThread().getContextClassLoader();
      // try to enter the right server apartment
      if (engineConfig != null
         && engineConfig instanceof XMLResourceProvider) {
         XMLResourceProvider config = (XMLResourceProvider) engineConfig;
         ClassLoader newLoader =
            config.getMyDeployment().getClassLoader(
               new QName(null, service.getName()));
         Thread.currentThread().setContextClassLoader(newLoader);
      }
      // here comes the reflective part
      try {
         // first some option preparation
         String allowedMethods =
            (String) service.getOption(allowedMethodsOption);
         String allowedReadAttributes =
            (String) service.getOption(allowedReadAttributesOption);
         String allowedWriteAttributes =
            (String) service.getOption(allowedWriteAttributesOption);
         String objectName =
            (String) service.getOption(Constants.OBJECTNAME_PROPERTY);
         String serverId =
            (String) service.getOption(Constants.MBEAN_SERVER_ID_PROPERTY);
         // process server id in order to find the right mbean server
         try
         {
            server = MBeanServerLocator.locateJBoss();
         }
         catch (IllegalStateException e)
         {
            throw new AxisFault(Constants.NO_MBEAN_SERVER_FOUND);
         }
         // build objectname
         name = new ObjectName(objectName);
         MBeanInfo info = server.getMBeanInfo(name);
         ServiceDesc serviceDesc = service.getServiceDescription();
         // first we set it to "complete" such that no
         java.lang.reflect.Field completeField =
            ServiceDesc.class.getDeclaredField("introspectionComplete");
         completeField.setAccessible(true);
         completeField.set(serviceDesc, Boolean.TRUE);
         Class implClass =
            org.jboss.util.Classes.loadClass(info.getClassName());
         serviceDesc.setImplClass(implClass);
         serviceDesc.setTypeMapping(
            (TypeMapping) service.getTypeMappingRegistry().getTypeMapping(
               org.apache.axis.Constants.URI_DEFAULT_SOAP_ENC));
         // first inspect operations
         MBeanOperationInfo[] operations = info.getOperations();
         for (int count = 0; count < operations.length; count++) {
            String operationName = operations[count].getName();
            // is this method exposed?
            if (allowedMethods != null
               && allowedMethods.equals("*")
               || allowedMethods.indexOf(operationName + " ") != -1
               || allowedMethods.indexOf(" " + operationName) != -1
               || allowedMethods.equals(operationName)) {
               // yes, we build an operation description
               OperationDesc opDesc = new OperationDesc();
               // check overloading, if already present, attach the method count
               if (methodData.containsKey(operationName)) {
                  operationName = operationName + count;
               }
               opDesc.setName(operationName);
               opDesc.setElementQName(new QName("", operationName));
               // we cache the mbean operation infos
               methodData.put(operationName, operations[count]);
               // process parameters
               MBeanParameterInfo[] parameters =
                  operations[count].getSignature();
               Class[] parameterTypes = new Class[parameters.length];
               for (int count2 = 0; count2 < parameters.length; count2++) {
                  ParameterDesc param = new ParameterDesc();
                  param.setName("arg" + count2);
                  parameterTypes[count2] =
                     forName(parameters[count2].getType());
                  param.setJavaType(parameterTypes[count2]);
                  param.setTypeQName(
                     forName(
                        parameterTypes[count2],
                        serviceDesc.getTypeMapping()));
                  opDesc.addParameter(param);
               }
               opDesc.setMethod(
                  implClass.getMethod(
                     operations[count].getName(),
                     parameterTypes));
               opDesc.setReturnClass(
                  forName(operations[count].getReturnType()));
               opDesc.setReturnType(
                  forName(
                     opDesc.getReturnClass(),
                     serviceDesc.getTypeMapping()));
               // TODO faultprocessing
               serviceDesc.addOperationDesc(opDesc);
            } // if
         } // for
         // next we do attribute processing
         MBeanAttributeInfo[] attributes = info.getAttributes();
         for (int count = 0; count < attributes.length; count++) {
            String attributeName = attributes[count].getName();
            // is the attribute readable?
            if (attributes[count].isReadable()
               && allowedReadAttributes != null
               && (allowedReadAttributes.equals("*")
                  || allowedReadAttributes.indexOf(attributeName + " ") != -1
                  || allowedReadAttributes.indexOf(" " + attributeName) != -1
                  || allowedReadAttributes.equals(attributeName))) {
               OperationDesc opDesc = new OperationDesc();
               if (attributes[count].getType().equals("boolean")) {
                  opDesc.setName("is" + attributeName);
               } else {
                  opDesc.setName("get" + attributeName);
               }
               if (methodData.containsKey(opDesc.getName())) {
                  opDesc.setName(opDesc.getName() + count + "A");
               }
               opDesc.setElementQName(new QName("", opDesc.getName()));
               attributeData.put(opDesc.getName(), attributes[count]);
               opDesc.setReturnClass(forName(attributes[count].getType()));
               opDesc.setReturnType(
                  forName(
                     opDesc.getReturnClass(),
                     serviceDesc.getTypeMapping()));
               // TODO faultprocessing
               serviceDesc.addOperationDesc(opDesc);
            } // if
            if (attributes[count].isWritable()
               && allowedWriteAttributes != null
               && (allowedWriteAttributes.equals("*")
                  || allowedWriteAttributes.indexOf(attributeName + " ") != -1
                  || allowedWriteAttributes.indexOf(" " + attributeName) != -1
                  || allowedWriteAttributes.equals(attributeName))) {
               OperationDesc opDesc = new OperationDesc();
               opDesc.setName("set" + attributeName);
               if (methodData.containsKey(opDesc.getName())) {
                  opDesc.setName(opDesc.getName() + count + "A");
               }
               opDesc.setElementQName(new QName("", opDesc.getName()));
               attributeData.put(opDesc.getName(), attributes[count]);
               ParameterDesc p = new ParameterDesc();
               p.setName("arg0");
               p.setJavaType(forName(attributes[count].getType()));
               p.setTypeQName(
                  forName(p.getJavaType(), serviceDesc.getTypeMapping()));
               opDesc.addParameter(p);
               opDesc.setReturnType(org.apache.axis.encoding.XMLType.AXIS_VOID);
               // TODO faultprocessing
               serviceDesc.addOperationDesc(opDesc);
            } // if
         } // for
      } catch (InstanceNotFoundException e) {
         throw new AxisFault(Constants.NO_MBEAN_INSTANCE, e);
      } catch (IntrospectionException e) {
         throw new AxisFault(Constants.INTROSPECTION_EXCEPTION, e);
      } catch (ReflectionException e) {
         throw new AxisFault(Constants.INTROSPECTION_EXCEPTION, e);
      } catch (ClassNotFoundException e) {
         throw new AxisFault(Constants.INTROSPECTION_EXCEPTION, e);
      } catch (NoSuchMethodException e) {
         throw new AxisFault(Constants.INTROSPECTION_EXCEPTION, e);
      } catch (MalformedObjectNameException e) {
         throw new AxisFault(Constants.WRONG_OBJECT_NAME, e);
      } catch (IllegalAccessException e) {
         throw new AxisFault(Constants.INTROSPECTION_EXCEPTION, e);
      } catch (NoSuchFieldException e) {
         throw new AxisFault(Constants.INTROSPECTION_EXCEPTION, e);
      } finally {
         Thread.currentThread().setContextClassLoader(currentLoader);
      }
   }

   /** resolve string-based jmx types */
   protected Class forName(String string) throws ClassNotFoundException {
      if ("void".equals(string)) {
         return void.class;
      } else if ("boolean".equals(string)) {
         return boolean.class;
      } else if ("float".equals(string)) {
         return float.class;
      } else if ("double".equals(string)) {
         return double.class;
      } else if ("int".equals(string)) {
         return int.class;
      } else if ("long".equals(string)) {
         return long.class;
      } else if ("short".equals(string)) {
         return short.class;
      } else if ("byte".equals(string)) {
         return byte.class;
      } else if ("char".equals(string)) {
         return char.class;
      } else {
         return org.jboss.util.Classes.loadClass(string);
      }
   }
   
   /** resolve string-based jmx types */
   protected QName forName(Class clazz, TypeMapping tm)
      throws ClassNotFoundException {
      if (void.class.equals(clazz)) {
         return org.apache.axis.encoding.XMLType.AXIS_VOID;
      } else {
         return tm.getTypeQName(clazz);
      }
   }

   /**
    * Invoke is called to do the actual work of the Handler object.
    */
   public void invoke(MessageContext msgContext) throws AxisFault {
      // the options of the service
      String serviceName = msgContext.getTargetService();
      // dissect the message
      Message reqMsg = msgContext.getRequestMessage();
      SOAPEnvelope reqEnv = (SOAPEnvelope) reqMsg.getSOAPEnvelope();
      Message resMsg = msgContext.getResponseMessage();
      SOAPEnvelope resEnv =
         (resMsg == null)
            ? new SOAPEnvelope()
            : (SOAPEnvelope) resMsg.getSOAPEnvelope();
      // copied code from RobJ, duh?
      if (msgContext.getResponseMessage() == null) {
         resMsg = new Message(resEnv);
         msgContext.setResponseMessage(resMsg);
      }
      // navigate the bodies
      Iterator allBodies = reqEnv.getBodyElements().iterator();
      while (allBodies.hasNext()) {
         Object nextBody = allBodies.next();
         if (nextBody instanceof RPCElement) {
            RPCElement body = (RPCElement) nextBody;
            String mName = body.getMethodName();
            List args = null;
            try {
               args = body.getParams();
            } catch (SAXException e) {
               throw new AxisFault(Constants.EXCEPTION_OCCURED, e);
            }
            Object result = null;
            try {
               MBeanAttributeInfo attr =
                  (MBeanAttributeInfo) attributeData.get(mName);
               if (attr != null) {
                  if (mName.startsWith("get") || mName.startsWith("is")) {
                     result = server.getAttribute(name, attr.getName());
                  } else {
                     RPCParam p = (RPCParam) args.get(0);
                     Object arg =
                        JavaUtils.convert(
                           p.getValue(),
                           forName(attr.getType()));
                     server.setAttribute(
                        name,
                        new Attribute(attr.getName(), arg));
                     result = null;
                  }
               } else {
                  MBeanOperationInfo meth =
                     (MBeanOperationInfo) methodData.get(mName);
                  MBeanParameterInfo[] params = meth.getSignature();
                  Object[] arguments = new Object[params.length];
                  String[] classNames = new String[params.length];
                  for (int count2 = 0; count2 < params.length; count2++) {
                     classNames[count2] = params[count2].getType();
                     if (args.size() > count2) {
                        RPCParam param = (RPCParam) args.get(count2);
                        arguments[count2] =
                           JavaUtils.convert(
                              param.getValue(),
                              forName(classNames[count2]));
                     } else {
                        arguments[count2] = null;
                     }
                  }
                  // now do the JMX call
                  result =
                     server.invoke(name, meth.getName(), arguments, classNames);
               }
               // and encode it back to the response
               RPCElement resBody = new RPCElement(mName + "Response");
               resBody.setPrefix(body.getPrefix());
               resBody.setNamespaceURI(body.getNamespaceURI());
               RPCParam param = new RPCParam(mName + "Result", result);
               resBody.addParam(param);
               resEnv.addBodyElement(resBody);
               resEnv.setEncodingStyle(
                  org.apache.axis.Constants.URI_DEFAULT_SOAP_ENC);
            } catch (InstanceNotFoundException e) {
               throw new AxisFault(Constants.NO_MBEAN_INSTANCE, e);
            } catch (AttributeNotFoundException e) {
               throw new AxisFault(Constants.NO_SUCH_ATTRIBUTE, e);
            } catch (InvalidAttributeValueException e) {
               throw new AxisFault(Constants.INVALID_ARGUMENT, e);
            } catch (MBeanException e) {
               throw new AxisFault(Constants.MBEAN_EXCEPTION, e);
            } catch (ClassNotFoundException e) {
               throw new AxisFault(Constants.CLASS_NOT_FOUND, e);
            } catch (ReflectionException e) {
               throw new AxisFault(
                  Constants.EXCEPTION_OCCURED,
                  e.getTargetException());
            } catch (SOAPException e) {
               throw new AxisFault(Constants.EXCEPTION_OCCURED, e);
            }
         } // if
      } // for
   }

   /** generate wsdl document from meta-data */
   public void generateWSDL(MessageContext msgCtx) throws AxisFault {
      EngineConfiguration engineConfig =
         msgCtx != null ? msgCtx.getAxisEngine().getConfig() : null;
      ClassLoader currentLoader =
         Thread.currentThread().getContextClassLoader();
      if (engineConfig != null
         && engineConfig instanceof XMLResourceProvider) {
         XMLResourceProvider config = (XMLResourceProvider) engineConfig;
         ClassLoader newLoader =
            config.getMyDeployment().getClassLoader(
               new QName(null, msgCtx.getTargetService()));
         Thread.currentThread().setContextClassLoader(newLoader);
      }
      SOAPService service = msgCtx.getService();
      ServiceDesc serviceDesc = service.getInitializedServiceDesc(msgCtx);
      // check whether there is an http action header present
      if (msgCtx != null) {
         boolean isSoapAction =
            msgCtx.getProperty(Constants.ACTION_HANDLER_PRESENT_PROPERTY)
               == Boolean.TRUE;
         // yes, then loop through the operation descriptions
         for (Iterator alloperations = serviceDesc.getOperations().iterator();
            alloperations.hasNext();
            ) {
            OperationDesc opDesc = (OperationDesc) alloperations.next();
            // and add a soap action tag with the name of the service
            opDesc.setSoapAction(isSoapAction ? service.getName() : null);
         }
      }
      try {
         // Location URL is whatever is explicitly set in the MC
         String locationUrl =
            msgCtx.getStrProp(MessageContext.WSDLGEN_SERV_LOC_URL);
         if (locationUrl == null) {
            // If nothing, try what's explicitly set in the ServiceDesc
            locationUrl = serviceDesc.getEndpointURL();
         }
         if (locationUrl == null) {
            // If nothing, use the actual transport URL
            locationUrl = msgCtx.getStrProp(MessageContext.TRANS_URL);
         }
         // Interface namespace is whatever is explicitly set
         String interfaceNamespace =
            msgCtx.getStrProp(MessageContext.WSDLGEN_INTFNAMESPACE);
         if (interfaceNamespace == null) {
            // If nothing, use the default namespace of the ServiceDesc
            interfaceNamespace = serviceDesc.getDefaultNamespace();
         }
         if (interfaceNamespace == null) {
            // If nothing still, use the location URL determined above
            interfaceNamespace = locationUrl;
         }
         Emitter emitter = new Emitter();
         String alias = (String) service.getOption("alias");
         if (alias != null)
            emitter.setServiceElementName(alias);
         // Set style/use
         emitter.setCls(serviceDesc.getImplClass());
         // If a wsdl target namespace was provided, use the targetNamespace.
         // Otherwise use the interfaceNamespace constructed above.
         String targetNamespace =
            (String) service.getOption(
               JavaProvider.OPTION_WSDL_TARGETNAMESPACE);
         if (targetNamespace == null || targetNamespace.length() == 0) {
            targetNamespace = interfaceNamespace;
         }
         emitter.setIntfNamespace(targetNamespace);
         emitter.setLocationUrl(locationUrl);
         emitter.setServiceDesc(serviceDesc);
         emitter.setTypeMapping(
            (TypeMapping) service.getTypeMappingRegistry().getTypeMapping(
               org.apache.axis.Constants.URI_DEFAULT_SOAP_ENC));
         emitter.setDefaultTypeMapping(
            (TypeMapping) msgCtx
               .getTypeMappingRegistry()
               .getDefaultTypeMapping());
         String wsdlPortType =
            (String) service.getOption(JavaProvider.OPTION_WSDL_PORTTYPE);
         String wsdlServiceElement =
            (String) service.getOption(JavaProvider.OPTION_WSDL_SERVICEELEMENT);
         String wsdlServicePort =
            (String) service.getOption(JavaProvider.OPTION_WSDL_SERVICEPORT);
         if (wsdlPortType != null && wsdlPortType.length() > 0) {
            emitter.setPortTypeName(wsdlPortType);
         }
         if (wsdlServiceElement != null && wsdlServiceElement.length() > 0) {
            emitter.setServiceElementName(wsdlServiceElement);
         }
         if (wsdlServicePort != null && wsdlServicePort.length() > 0) {
            emitter.setServicePortName(wsdlServicePort);
         }
         Definition def = emitter.getWSDL();
         def.addNamespace(
            "xsd99",
            org.apache.axis.Constants.URI_1999_SCHEMA_XSD);
         def.addNamespace(
            "xsd00",
            org.apache.axis.Constants.URI_2000_SCHEMA_XSD);
         Document doc =
            WSDLFactory.newInstance().newWSDLWriter().getDocument(def);
         java.lang.reflect.Field field =
            Emitter.class.getDeclaredField("types");
         field.setAccessible(true);
         ((Types) field.get(emitter)).insertTypesFragment(doc);
         msgCtx.setProperty("WSDL", doc);
      } catch (NoClassDefFoundError e) {
         log.info(Messages.getMessage("toAxisFault00"), e);
         throw new AxisFault(e.toString(), e);
      } catch (Exception e) {
         log.info(Messages.getMessage("toAxisFault00"), e);
         throw AxisFault.makeFault(e);
      }
   }

   /**
    * TODO called when a fault occurs to 'undo' whatever 'invoke' did.
    */
   public void undo(MessageContext msgContext) {
      // unbelievable this foresight
   }
}
