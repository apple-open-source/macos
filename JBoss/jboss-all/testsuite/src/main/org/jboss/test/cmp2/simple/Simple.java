package org.jboss.test.cmp2.simple;

import java.math.BigDecimal;
import java.sql.Time;
import java.sql.Timestamp;
import java.util.Hashtable;
import javax.ejb.EJBLocalObject;

public interface Simple extends EJBLocalObject {
   public String getId();

   public ValueClass getValueClass();
   public void setValueClass(ValueClass vc);

   public boolean getBooleanPrimitive();
   public void setBooleanPrimitive(boolean b);

   public Boolean getBooleanObject();
   public void setBooleanObject(Boolean b);

   public byte getBytePrimitive();
   public void setBytePrimitive(byte b);

   public Byte getByteObject();
   public void setByteObject(Byte b);

   public short getShortPrimitive();
   public void setShortPrimitive(short s);

   public Short getShortObject();
   public void setShortObject(Short s);

   public int getIntegerPrimitive();
   public void setIntegerPrimitive(int i);

   public Integer getIntegerObject();
   public void setIntegerObject(Integer i);

   public long getLongPrimitive();
   public void setLongPrimitive(long l);

   public Long getLongObject();
   public void setLongObject(Long l);

   public float getFloatPrimitive();
   public void setFloatPrimitive(float f);

   public Float getFloatObject();
   public void setFloatObject(Float f);

   public double getDoublePrimitive();
   public void setDoublePrimitive(double d);

   public Double getDoubleObject();
   public void setDoubleObject(Double d);

   public String getStringValue();
   public void setStringValue(String s);

   public java.util.Date getUtilDateValue();
   public void setUtilDateValue(java.util.Date d);
   public void updateUtilDateValue(java.util.Date d);

   public java.sql.Date getSqlDateValue();
   public void setSqlDateValue(java.sql.Date d);

   public Time getTimeValue();
   public void setTimeValue(Time t);

   public Timestamp getTimestampValue();
   public void setTimestampValue(Timestamp t);

   public java.math.BigDecimal getBigDecimalValue();
   public void setBigDecimalValue(java.math.BigDecimal d);

   public byte[] getByteArrayValue();
   public void setByteArrayValue(byte[] bytes);

   public Object getObjectValue();
   public void setObjectValue(Object t);

   public void addToHashtable(String key, String value);
   public Hashtable getHashtable();
   public void setHashtable(Hashtable t);
}
