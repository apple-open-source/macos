package org.jbossmx.util;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;

public class ObjectNameMetaData
{
    private ObjectNameMetaData() {}

    public ObjectNameMetaData(String objectName)
        throws MalformedObjectNameException
    {
        this(new ObjectName(objectName));
    }

    public ObjectNameMetaData(ObjectName objectName)
    {
        m_objectName = objectName;
        m_metaData = new HashMap();
    }

    public ObjectName getObjectName()
    {
        return m_objectName;
    }

    public Object getMetaData(Object key)
    {
        return m_metaData.get(key);
    }

    public void setMetaData(Object key, Object value)
    {
        System.out.println("ObjectNameMetaData.setMetaData(key = " + key + ", value = " + value + ")");
        m_metaData.put(key, value);
    }

    public String toString()
    {
        StringBuffer sb = new StringBuffer();
        sb.append("ObjectNameMetaData(" + m_objectName + ")\n");

        for (Iterator iterator = m_metaData.entrySet().iterator(); iterator.hasNext();)
        {
            final Map.Entry entry = (Map.Entry) iterator.next();
            sb.append("key = " + entry.getKey() + ", value = " + entry.getValue() + "\n");
        }

        return sb.toString();
    }

    private ObjectName m_objectName;
    private Map m_metaData;
}
