/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package java.sql;

/**
 * A HACK to allow building under JDK 1.3.
 */
public interface ParameterMetaData 
{
   int getParameterCount() throws SQLException;

   int isNullable(int param) throws SQLException;

   int parameterNoNulls = 0;

   int parameterNullable = 1;

   int parameterNullableUnknown = 2;

   boolean isSigned(int param) throws SQLException;

   int getPrecision(int param) throws SQLException;

   int getScale(int param) throws SQLException;	

   int getParameterType(int param) throws SQLException;

   String getParameterTypeName(int param) throws SQLException;

   String getParameterClassName(int param) throws SQLException;

   int parameterModeUnknown = 0;

   int parameterModeIn = 1;

   int parameterModeInOut = 2;

   int parameterModeOut = 4;

   int getParameterMode(int param) throws SQLException;
}
