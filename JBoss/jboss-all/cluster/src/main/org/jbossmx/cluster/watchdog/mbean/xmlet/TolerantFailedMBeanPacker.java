package org.jbossmx.cluster.watchdog.mbean.xmlet;

/**
 * Title:
 * Description:
 * Copyright:    Copyright (c) 2001
 * Company:
 * @author Stacy Curl
 * @version 1.0
 */

public class TolerantFailedMBeanPacker implements FailedMBeanPacker
{
    public TolerantFailedMBeanPacker() {}

    public Object packFailedMBean(final XMLetEntry xmletEntry, Throwable throwable)
    {
        return null;
    }
}