#!/bin/sh

#test=cmp2
#test=jca
#test=jbossmq
#test=testbean
#test=naming
#test=security
#target=test

#test=org.jboss.test.bank.test.BankStressTestCase
#test=org.jboss.test.bankiiop.test.BankStressTestCase
#test=org.jboss.test.classloader.test.UnifiedLoaderTestCase
#test=org.jboss.test.classloader.test.CircularityUnitTestCase
#test=org.jboss.test.classloader.test.ScopingUnitTestCase
#test=org.jboss.test.cmp2.readonly.ReadonlyUnitTestCase
#test=org.jboss.test.cmp2.cmr.test.CMRPostCreatesWrittenUnitTestCase
#test=org.jboss.test.cmp2.optimisticlock.test.OptimisticLockUnitTestCase
#test=org.jboss.test.cmp2.perf.test.PerfUnitTestCase
#test=org.jboss.test.cmp2.simple.SimpleUnitTestCase
#test=org.jboss.test.cts.test.BmpUnitTestCase
#test=org.jboss.test.cts.test.CtsCmp2UnitTestCase
#test=org.jboss.test.cts.test.ExceptionUnitTestCase
#test=org.jboss.test.cts.test.IndependentJarsUnitTestCase
#test=org.jboss.test.cts.test.MDBUnitTestCase
#test=org.jboss.test.cts.test.StatefulSessionUnitTestCase
#test=org.jboss.test.cts.test.StatelessSessionUnitTestCase
#test=org.jboss.test.ejbconf.test.MetaDataUnitTestCase
#test=org.jboss.test.hello.test.HelloClusteredHttpStressTestCase

#test=org.jboss.test.hello.test.HelloHttpStressTestCase
#test=org.jboss.test.helloiiop.test.HelloTimingStressTestCase
#test=org.jboss.test.jbossmq.test.ConnectionStressTestCase
#test=org.jboss.test.jbossmq.test.ConnectionUnitTestCase
#test=org.jboss.test.jbossmq.perf.InvocationLayerStressTestCase
#test=org.jboss.test.jbossmq.test.JBossMQUnitTestCase
test=org.jboss.test.jbossmq.perf.JBossMQPerfStressTestCase
#test=org.jboss.test.jbossmq.test.JBossSessionRecoverUnitTestCase
#test=org.jboss.test.jbossmq.test.SecurityUnitTestCase
#test=org.jboss.test.jbossmx.compliance.timer.BasicTestCase
#test=org.jboss.test.jbossmx.implementation.server.ContextCLTestCase
#test=org.jboss.test.jca.test.BaseConnectionManagerUnitTestCase
#test=org.jboss.test.jca.test.ConnectionFactorySerializationUnitTestCase
#test=org.jboss.test.jca.test.JDBCStatementTestsConnectionUnitTestCase
#test=org.jboss.test.jca.test.LocalWrapperCleanupUnitTestCase
#test=org.jboss.test.jmx.test.DeployServiceUnitTestCase
#test=org.jboss.test.jmx.test.DeployXMBeanUnitTestCase
#test=org.jboss.test.jmx.test.EarDeploymentUnitTestCase
#test=org.jboss.test.jmx.test.JarInSarJSR77UnitTestCase
#test=org.jboss.test.jmx.test.JMXInvokerUnitTestCase
#test=org.jboss.test.jmx.test.ServiceRsrcsUnitTestCase
#test=org.jboss.test.jmx.test.CPManifestUnitTestCase
#test=org.jboss.test.jmx.test.UndeployBrokenPackageUnitTestCase
#test=org.jboss.test.jmx.test.UnpackedDeploymentUnitTestCase
#test=org.jboss.test.management.test.JSR77SpecUnitTestCase
#test=org.jboss.test.naming.test.EjbLinkUnitTestCase
#test=org.jboss.test.naming.test.ENCUnitTestCase
#test=org.jboss.test.naming.test.SecurityUnitTestCase
#test=org.jboss.test.naming.test.SimpleUnitTestCase
#test=org.jboss.test.perf.test.PerfUnitTestCase
#test=org.jboss.test.perf.test.PerfStressTestCase
#test=org.jboss.test.perf.test.SecurePerfStressTestCase
#test=org.jboss.test.readahead.test.ReadAheadUnitTestCase
#test=org.jboss.test.security.test.EJBSpecUnitTestCase
#test=org.jboss.test.security.test.HttpsUnitTestCase
#test=org.jboss.test.security.test.SRPUnitTestCase
#test=org.jboss.test.security.test.LoginModulesUnitTestCase
#test=org.jboss.test.security.test.SecurityProxyUnitTestCase
#test=org.jboss.test.security.test.SRPLoginModuleUnitTestCase
#test=org.jboss.test.security.test.SRPUnitTestCase
#test=org.jboss.test.security.test.XMLLoginModulesUnitTestCase
#test=org.jboss.test.security.test.PasswordHashLoginUnitTestCase
#test=org.jboss.test.testbean.test.BeanUnitTestCase
#test=org.jboss.test.testbeancluster.test.BeanUnitTestCase
#test=org.jboss.test.util.test.PropertyEditorsUnitTestCase
#test=org.jboss.test.util.test.ProtocolHandlerUnitTestCase
#test=org.jboss.test.util.test.SchedulerUnitTestCase
#test=org.jboss.test.web.test.WebIntegrationUnitTestCase
target=one-test

/bin/rm -rf output/reports
#junit_opts="-Djunit.timeout=180000 -Dnojars=t -Djbosstest.nodeploy=true"
junit_opts="-Djunit.timeout=180000 -Dnojars=t -Djbosstest.iterationcount=500"
#junit_opts="-Djunit.timeout=1800000 -Dnojars=t"
ant -Dtest=$test $junit_opts $target tests-report

