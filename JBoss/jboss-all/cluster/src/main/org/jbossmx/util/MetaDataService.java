package org.jbossmx.util;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import javax.management.ObjectName;

public class MetaDataService implements MetaDataServiceMBean
{
  public MetaDataService()
  {
    m_metaData = new HashMap();
  }

  public ObjectNameMetaData getMetaData(ObjectName objectName)
  {
    return (ObjectNameMetaData) m_metaData.get(objectName);
  }

  public void setMetaData(ObjectNameMetaData objectNameMetaData)
  {
    m_metaData.put(objectNameMetaData.getObjectName(), objectNameMetaData);
  }

  public String listMetaData()
  {
    StringBuffer sb = new StringBuffer();

    for (Iterator iterator = m_metaData.values().iterator(); iterator.hasNext();)
    {
        ObjectNameMetaData objectNameMetaData =
            (ObjectNameMetaData) iterator.next();

        sb.append(objectNameMetaData.toString());
    }

    return sb.toString();
  }


  private Map m_metaData;
}
