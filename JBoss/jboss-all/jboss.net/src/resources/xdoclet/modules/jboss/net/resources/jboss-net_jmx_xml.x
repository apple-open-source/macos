<XDtMerge:merge file="xdoclet/modules/jboss/net/resources/jboss-net_xml_head.xdt">
</XDtMerge:merge>

<!-- The following are declarations of service endpoints targetted to
     mbeans -->
     
<XDtClass:forAllClasses>
<XDtClass:ifHasClassTag tagName="jmx:mbean" paramName="name">
<XDtClass:ifHasClassTag tagName="jboss-net:web-service">
  <service name="<XDtClass:classTagValue tagName='jboss-net:web-service' paramName='urn'/>" provider="Handler">
    <parameter name="handlerClass" value="org.jboss.net.jmx.server.MBeanProvider"/>
    <parameter name="ObjectName" value="<XDtClass:classTagValue tagName='jmx:mbean' paramName='name'/>"/>
    <parameter name="allowedMethods" value="<XDtClass:ifHasClassTag tagName="jboss-net:web-service" paramName="expose-all">*</XDtClass:ifHasClassTag><XDtClass:ifDoesntHaveClassTag tagName="jboss-net:web-service" paramName="expose-all"><XDtMethod:forAllMethods><XDtMethod:ifHasMethodTag tagName="jmx:managed-operation"><XDtMethod:ifHasMethodTag tagName="jboss-net:web-method"><XDtMethod:methodName/> </XDtMethod:ifHasMethodTag></XDtMethod:ifHasMethodTag></XDtMethod:forAllMethods></XDtClass:ifDoesntHaveClassTag>"/>
    <parameter name="allowedReadAttributes" value="<XDtClass:ifHasClassTag tagName="jboss-net:web-service" paramName="expose-all">*</XDtClass:ifHasClassTag><XDtClass:ifDoesntHaveClassTag tagName="jboss-net:web-service" paramName="expose-all"><XDtMethod:forAllMethods><XDtMethod:ifHasMethodTag tagName="jmx:managed-attribute"><XDtMethod:ifIsGetter><XDtMethod:ifHasMethodTag tagName="jboss-net:web-method"><XDtMethod:methodNameWithoutPrefix/> </XDtMethod:ifHasMethodTag></XDtMethod:ifIsGetter></XDtMethod:ifHasMethodTag></XDtMethod:forAllMethods></XDtClass:ifDoesntHaveClassTag>"/>
    <parameter name="allowedWriteAttributes" value="<XDtClass:ifHasClassTag tagName="jboss-net:web-service" paramName="expose-all">*</XDtClass:ifHasClassTag><XDtClass:ifDoesntHaveClassTag tagName="jboss-net:web-service" paramName="expose-all"><XDtMethod:forAllMethods><XDtMethod:ifHasMethodTag tagName="jmx:managed-attribute"><XDtMethod:ifIsSetter><XDtMethod:ifHasMethodTag tagName="jboss-net:web-method"><XDtMethod:methodNameWithoutPrefix/> </XDtMethod:ifHasMethodTag></XDtMethod:ifIsSetter></XDtMethod:ifHasMethodTag></XDtMethod:forAllMethods></XDtClass:ifDoesntHaveClassTag>"/>
  </service>
</XDtClass:ifHasClassTag>
</XDtClass:ifHasClassTag>
</XDtClass:forAllClasses>

<XDtMerge:merge file="xdoclet/modules/jboss/net/resources/jboss-net_xml_tail.xdt">
</XDtMerge:merge>


