package org.jbossmx.cluster.watchdog.mbean.xmlet;

public interface FailedMBeanPacker
{
    public Object packFailedMBean(final XMLetEntry xmletEntry, Throwable throwable);
}