package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.io.IOException;
import java.io.StringReader;
import java.util.List;
import java.util.ArrayList;
import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;
import org.w3c.dom.Element;

public final class JDBCFunctionMappingMetaData {
   private final String functionName;
   private String[] sqlChunks;
   private int[] parameters;

   public JDBCFunctionMappingMetaData(String functionName, String[] sqlChunks, int[] parameters) {
      this.functionName = functionName;
      this.sqlChunks = sqlChunks;
      this.parameters = parameters;
   }

   public JDBCFunctionMappingMetaData(Element element) throws DeploymentException {
      functionName = MetaData.getUniqueChildContent(element, "function-name");
      
      String sql = MetaData.getUniqueChildContent(element, "function-sql");
      initFromString(sql);
   }
   
   public JDBCFunctionMappingMetaData(String functionName, String sql) throws DeploymentException
   {
      this.functionName = functionName;
      initFromString(sql);
   }

   protected void initFromString(String sql) throws DeploymentException
   {
      ArrayList chunkList = new ArrayList();      
      ArrayList parameterList = new ArrayList();      
      
      // add a dummy chunk so we can be assured that the sql started with chunk before a number
      if(sql.charAt(0) == '?') {
         chunkList.add("");
      }
      // break the sql into chunks and parameters
      StringBuffer chunk = new StringBuffer();
      StringReader reader = new StringReader(sql);
      try {
         for(int c=reader.read(); c >= 0; c=reader.read()) {
            if(c != '?') {
               chunk.append((char)c);
            } else {
               chunkList.add(chunk.toString());
               chunk = new StringBuffer();
               
               // read the number
               StringBuffer number = new StringBuffer();
               for(int digit=reader.read(); digit >= 0; digit=reader.read()) {
                  if(Character.isDigit((char)digit)) {
                     number.append((char)digit);
                  } else {
                     if(digit >= 0) {
                        chunk.append((char)digit);
                     }
                     break;
                  }
               }
               if(number.length() == 0) {
                  throw new DeploymentException("Invalid parameter in function-sql: "+sql);
               }
               Integer parameter = null;
               try {
                  parameter = new Integer(number.toString());
               } catch(NumberFormatException e) {
                  throw new DeploymentException("Invalid parameter number in function-sql: number="+number+" sql="+sql);
               }
               parameterList.add(parameter);
            }
         }
      } catch(IOException e) {
         // will never happen because io is in memory, but required by the interface
         throw new DeploymentException("Error parsing function-sql: " + sql);
      }
      chunkList.add(chunk.toString());
      
      // save out the chunks
      sqlChunks = new String[chunkList.size()];
      chunkList.toArray(sqlChunks);
      
      // save out the parameter order
      parameters = new int[parameterList.size()];
      for(int i=0; i<parameters.length; i++) {
         parameters[i] = ((Integer)parameterList.get(i)).intValue()-1;
      }            
   }

   public String getFunctionName() {
      return functionName;
   }
   
   public String getFunctionSql(Object[] args) {
      StringBuffer buf = new StringBuffer();
      
      for(int i=0; i<sqlChunks.length; i++) {
         buf.append(sqlChunks[i]);
         if(i<parameters.length) {
            buf.append(args[parameters[i]]);
         }
      }
      return buf.toString();
   }
   
   public List getParameters() {
      ArrayList list = new ArrayList(parameters.length);
      for(int i=0; i<parameters.length; i++) {
         list.add(new Integer(parameters[i]));
      }
      return list;
   }
}
