package org.jbossmx.util;

import javax.management.ObjectName;

import org.jboss.mx.util.ObjectNameFactory;

public interface MetaDataServiceMBean
{
   // Constants
   ObjectName OBJECT_NAME = ObjectNameFactory.create("jboss:name=MetaDataService");

   public ObjectNameMetaData getMetaData(ObjectName objectName);
   public void setMetaData(ObjectNameMetaData objectNameMetaData);

   public String listMetaData();
}
