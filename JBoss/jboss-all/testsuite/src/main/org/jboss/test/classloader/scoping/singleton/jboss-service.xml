
<!-- Configure each deployment of the TestService to have a unique scoped
loader repository.
 -->
<server>
   <loader-repository>
      jboss.test:loader=singleton.sar,version=@VERSION@
   </loader-repository>

   <mbean code="org.jboss.test.classloader.scoping.singleton.TestService"
      name="jboss.test:service=TestService,version=@VERSION@" />
</server>
