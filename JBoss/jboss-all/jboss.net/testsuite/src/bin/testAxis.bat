set JBOSS_CLASSPATH=.;%JBOSS_CLASSPATH%;..\lib\axis.jar;..\lib\log4j.jar;..\server\default\deploy\jbossdotnet.sar;..\lib\crimson.jar;..\lib\jboss-jmx.jar

java -classpath "%JBOSS_CLASSPATH%" org.jboss.net.axis.AxisInvocationHandler http://localhost:8080/axis/services  
java -classpath "%JBOSS_CLASSPATH%" org.jboss.net.jmx.MBeanInvocationHandler http://localhost:8080/axis/services 
java -classpath "%JBOSS_CLASSPATH%" org.jboss.net.jmx.connector.ConnectorInvocationHandler http://localhost:8080/axis/services 


