<XDtMerge:merge file="xdoclet/modules/jboss/net/resources/jboss-net_xml_head.xdt">
</XDtMerge:merge>

<!-- The following are declarations of service endpoints targetted to
     session beans -->
     
<XDtClass:forAllClasses type="javax.ejb.SessionBean">
 <XDtClass:ifHasClassTag tagName="jboss-net:web-service">
  <service name="<XDtClass:classTagValue tagName='jboss-net:web-service' paramName='urn'/>" provider="Handler">
    <XDtClass:ifHasClassTag tagName="jboss-net:web-service" paramName="scope">
     <parameter name="scope" value="<XDtClass:classTagValue tagName='jboss-net:web-service' paramName='scope'/>"/>
    </XDtClass:ifHasClassTag>
    <XDtClass:ifDoesntHaveClassTag tagName="jboss-net:web-service" paramName="scope">
    <XDtClass:ifClassTagValueEquals tagName="ejb:bean" paramName="type" value="Stateful">
     <parameter name="scope" value="Session"/>
    </XDtClass:ifClassTagValueEquals>
    </XDtClass:ifDoesntHaveClassTag>
    <parameter name="handlerClass" value="org.jboss.net.axis.server.EJBProvider"/>
    <parameter name="beanJndiName" value="<XDtEjb:ifLocalEjb><XDtEjbHome:jndiName type="local"/></XDtEjb:ifLocalEjb><XDtEjb:ifNotLocalEjb><XDtEjbHome:jndiName type="remote"/></XDtEjb:ifNotLocalEjb>"/>
    <parameter name="allowedMethods" value="<XDtClass:ifHasClassTag tagName="jboss-net:web-service" paramName="expose-all">*</XDtClass:ifHasClassTag><XDtClass:ifDoesntHaveClassTag tagName="jboss-net:web-service" paramName="expose-all"><XDtMethod:forAllMethods><XDtEjbIntf:ifIsInterfaceMethod><XDtMethod:ifHasMethodTag tagName="jboss-net:web-method"><XDtEjbIntf:interfaceMethodName/> </XDtMethod:ifHasMethodTag></XDtEjbIntf:ifIsInterfaceMethod></XDtMethod:forAllMethods></XDtClass:ifDoesntHaveClassTag>"/>
    <requestFlow name="<XDtClass:classTagValue tagName='jboss-net:web-service' paramName='urn'/>Request">
      
    <XDtClass:ifHasClassTag tagName="jboss-net:authentication">
      <handler type="java:org.jboss.net.axis.server.JBossAuthenticationHandler">
        <parameter name="securityDomain" value="java:/jaas/<XDtClass:classTagValue tagName='jboss-net:authentication' paramName='domain'/>"/>
      </handler>
    </XDtClass:ifHasClassTag>
    <XDtClass:ifHasClassTag tagName="jboss-net:authorization">
      <handler type="java:org.jboss.net.axis.server.JBossAuthorizationHandler">
        <parameter name="securityDomain" value="java:/jaas/<XDtClass:classTagValue tagName='jboss-net:authorization' paramName='domain'/>"/>
      <XDtClass:ifHasClassTag tagName="jboss-net:authorization" paramName='roles-allowed'>
        <parameter name="allowedRoles" value="<XDtClass:classTagValue tagName='jboss-net:authorization' paramName='roles-allowed'/>"/>
      </XDtClass:ifHasClassTag>
      <XDtClass:ifHasClassTag tagName="jboss-net:authorization" paramName='roles-denied'>
        <parameter name="deniedRoles" value="<XDtClass:classTagValue tagName='jboss-net:authorization' paramName='roles-denied'/>"/>
      </XDtClass:ifHasClassTag>
      </handler>
    </XDtClass:ifHasClassTag>
    <XDtClass:ifHasClassTag tagName="jboss-net:processEntities">
      <!-- this is a temporary solution to allow immediate (de-)serialization of entity beans in the web service layer --> 
      <handler type="java:org.jboss.net.axis.server.TransactionRequestHandler"/>
    </XDtClass:ifHasClassTag>
    </requestFlow>
    <responseFlow name="<XDtClass:classTagValue tagName='jboss-net:web-service' paramName='urn'/>Response">
    <XDtClass:ifHasClassTag tagName="jboss-net:processEntities">
      <!-- this is a temporary solution to allow immediate (de-)serialization of entity beans in the web service layer --> 
      <handler type="java:org.jboss.net.axis.server.SerialisationResponseHandler"/>
      <handler type="java:org.jboss.net.axis.server.TransactionResponseHandler"/>
    </XDtClass:ifHasClassTag>
    </responseFlow>
  </service>
 </XDtClass:ifHasClassTag>
</XDtClass:forAllClasses>

<!-- The following are typemappings for entity beans for implementing 
     the implicit web-service value-object pattern -->

<XDtClass:forAllClasses type="javax.ejb.EntityBean">
 <XDtClass:ifHasClassTag tagName="jboss-net:xml-schema">
  <XDtClass:ifDoesntHaveClassTag tagName="jboss-net:xml-schema" paramName="data-object">
  <typeMapping 
      qname="<XDtClass:classTagValue tagName='jboss-net:xml-schema' paramName='urn'/>" 
      type="java:<XDtEjb:ifRemoteEjb><XDtEjbIntf:componentInterface type="remote"/></XDtEjb:ifRemoteEjb><XDtEjb:ifLocalEjb><XDtEjbIntf:componentInterface type="local"/></XDtEjb:ifLocalEjb>"
      serializer="org.apache.axis.encoding.ser.BeanSerializerFactory"
      deserializer="org.jboss.net.axis.server.EntityBeanDeserializerFactory"
      encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"
  >
    <parameter name="JndiName" value="<XDtEjb:ifRemoteEjb><XDtEjbHome:jndiName type="remote"/></XDtEjb:ifRemoteEjb><XDtEjb:ifLocalEjb><XDtEjbHome:jndiName type="local"/></XDtEjb:ifLocalEjb>"/>
    <parameter name="FindMethodName" value="findByPrimaryKey"/>
    <parameter name="FindMethodElements" value="<XDtEjbPersistent:forAllPersistentFields superclasses="false" only-pk="true"><XDtMethod:propertyName/>;</XDtEjbPersistent:forAllPersistentFields>"/>
    <parameter name="FindMethodSignature" value="<XDtEjbPk:pkClass/>"/> 	
  </typeMapping>
  </XDtClass:ifDoesntHaveClassTag>
  <XDtClass:ifHasClassTag tagName="jboss-net:xml-schema" paramName="data-object">
  <typeMapping 
      qname="<XDtClass:classTagValue tagName='jboss-net:xml-schema' paramName='urn'/>" 
      type="java:<XDtEjbDataObj:dataObjectClass/>"
      serializer="org.apache.axis.encoding.ser.BeanSerializerFactory"
      deserializer="org.apache.axis.encoding.ser.BeanDeserializerFactory"
      encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"
  />
  </XDtClass:ifHasClassTag>
 </XDtClass:ifHasClassTag>
</XDtClass:forAllClasses>

<XDtMerge:merge file="xdoclet/modules/jboss/net/resources/jboss-net_xml_tail.xdt">
</XDtMerge:merge>


