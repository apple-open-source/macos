package org.jbossmx.cluster.watchdog.mbean.xmlet;

/**
 * Title:
 * Description:
 * Copyright:    Copyright (c) 2001
 * Company:
 * @author Stacy Curl
 * @version 1.0
 */

public class SimpleFailedMBeanPacker implements FailedMBeanPacker
{
    public Object packFailedMBean(final XMLetEntry xmletEntry, Throwable throwable)
    {
        return new Object[] { xmletEntry, throwable };
    }
}