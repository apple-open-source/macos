/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.jdbc;

import java.lang.ref.WeakReference;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.*;

import org.jboss.ejb.EntityContainer;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCEntityBridge;
import org.jboss.ejb.plugins.cmp.bridge.EntityBridgeInvocationHandler;
import org.jboss.ejb.plugins.cmp.bridge.FieldBridge;
import org.jboss.ejb.plugins.cmp.bridge.SelectorBridge;
import org.jboss.proxy.compiler.Proxy;
import org.jboss.proxy.compiler.InvocationHandler;
import org.jboss.deployment.DeploymentException;

/**
 * JDBCBeanClassInstanceCommand creates instance of the bean class. For 
 * CMP 2.0 it creates an instance of a subclass of the bean class, as the
 * bean class is abstract.
 *
 * <FIX-ME>should not generat a subclass for ejb 1.1</FIX-ME>
 *    
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.5.2.3 $
 */
 
public final class JDBCCreateBeanClassInstanceCommand {
   private final WeakReference container;
   private final JDBCEntityBridge entityBridge;
   private final Class beanClass;
   private final Constructor beanProxyConstructor;
   private final Map fieldMap;
   private final Map selectorMap;

   public JDBCCreateBeanClassInstanceCommand(JDBCStoreManager manager)
         throws Exception
   {
      EntityContainer theContainer = manager.getContainer();
      container = new WeakReference(theContainer);
      entityBridge = manager.getEntityBridge();
      beanClass = theContainer.getBeanClass();
      fieldMap = createFieldMap();
      selectorMap = createSelectorMap();
      // use proxy generator to create one implementation
      EntityBridgeInvocationHandler handler = new EntityBridgeInvocationHandler(
            theContainer,
            fieldMap,
            selectorMap,
            beanClass);
      Class[] classes = new Class[] { beanClass };
      ClassLoader classLoader = beanClass.getClassLoader();

      Object o = Proxy.newProxyInstance(classLoader, classes, handler);

      // steal the constructor from the object
      beanProxyConstructor =
            o.getClass().getConstructor(new Class[]{InvocationHandler.class});

      // now create one to make sure everything is cool
      execute();
   }

   public void destroy()
   {
      Proxy.forgetProxyForClass(beanClass);
   }
   
   public Object execute() throws Exception
   {
      EntityContainer theContainer = (EntityContainer) container.get();
      EntityBridgeInvocationHandler handler = new EntityBridgeInvocationHandler(
            theContainer,
            fieldMap,
            selectorMap,
            beanClass);

      return beanProxyConstructor.newInstance(new Object[]{handler});
   }

   private Map getAbstractAccessors() {
      Method[] methods = beanClass.getMethods();
      Map abstractAccessors = new HashMap(methods.length);

      for(int i=0; i<methods.length; i++) {
          if(Modifier.isAbstract(methods[i].getModifiers())) {
            String methodName = methods[i].getName();
            if(methodName.startsWith("get") || methodName.startsWith("set")) {
               abstractAccessors.put(methodName, methods[i]);
            }
         }
      }
      return abstractAccessors;
   }

   private Map createFieldMap() throws DeploymentException {

      Map abstractAccessors = getAbstractAccessors();

      List fields = entityBridge.getFields();
      Map map = new HashMap(fields.size() * 2);
      for(int i = 0; i < fields.size(); i++) {
         FieldBridge field = (FieldBridge)fields.get(i);

         // get the names
         String fieldName = field.getFieldName();
         String fieldBaseName = Character.toUpperCase(fieldName.charAt(0)) +
            fieldName.substring(1);
         String getterName = "get" + fieldBaseName;
         String setterName = "set" + fieldBaseName;

         // get the accessor methods
         Method getterMethod = (Method)abstractAccessors.get(getterName);
         Method setterMethod = (Method)abstractAccessors.get(setterName);

         // getters and setters must come in pairs
         if(getterMethod != null && setterMethod == null) {
            throw new DeploymentException("Getter was found but, no setter " +
                  "was found for field: " + fieldName);
         } else if(getterMethod == null && setterMethod != null) {
            throw new DeploymentException("Setter was found but, no getter " +
                  "was found for field: " + fieldName);
         } else if(getterMethod != null && setterMethod != null) {
            // add methods
            map.put(getterMethod, field);
            map.put(setterMethod, field);

            // remove the accessors (they have been used)
            abstractAccessors.remove(getterName);
            abstractAccessors.remove(setterName);
         }
      }
      return Collections.unmodifiableMap(map);
   }

   private Map createSelectorMap() {
      Collection selectors = entityBridge.getSelectors();
      Map map = new HashMap(selectors.size());
      for(Iterator iter = selectors.iterator(); iter.hasNext();) {
         SelectorBridge selector = (SelectorBridge)iter.next();
         map.put(selector.getMethod(), selector);
      }
      return Collections.unmodifiableMap(map);
   }
}
