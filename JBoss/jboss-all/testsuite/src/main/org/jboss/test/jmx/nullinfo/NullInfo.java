package org.jboss.test.jmx.nullinfo;

import javax.management.*;

public class NullInfo implements DynamicMBean{

    static org.apache.log4j.Category log =
       org.apache.log4j.Category.getInstance(NullInfo.class);
   
   //This is what kills it, the others aren't even called
   public MBeanInfo getMBeanInfo(){
      log.debug("Returning null from getMBeanInfo");
      return null;
   }
   
   public Object invoke(String action, Object[] params, String[] sig){
      log.debug("Returning null from invoke");
      return null;
   }
   
   public Object getAttribute(String attrName){
      log.debug("Returning null from getAttribute");
      return null;
   }
   public AttributeList getAttributes(String[] attrs){
      log.debug("Returning null from getAttributes");
      return null;
   }
   public void setAttribute(Attribute attr){
      log.debug("setAttribute called");
   }
   public AttributeList setAttributes(AttributeList attrs){
      log.debug("Returning null from setAttributes");
      return null;
   }
}
