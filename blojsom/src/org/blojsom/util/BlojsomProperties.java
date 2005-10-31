/**
 * Copyright (c) 2003-2005 , David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005  by Mark Lussier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation 
 *      and/or other materials provided with the distribution.
 * Neither the name of the "David A. Czarnecki" and "blojsom" nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * Products derived from this software may not be called "blojsom",
 * nor may "blojsom" appear in their name, without prior written permission of
 * David A. Czarnecki.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package org.blojsom.util;

import java.io.*;
import java.util.*;

/**
 * BlojsomProperties
 * <p/>
 * Saving properties in non-ISO encodings, based on java.util.Properties
 *
 * @author David Czarnecki
 * @author Jorg Prante
 * @version $Id: BlojsomProperties.java,v 1.2.2.1 2005/07/21 14:11:04 johnan Exp $
 * @since blojsom 2.01
 */
public class BlojsomProperties extends Properties implements BlojsomConstants {

    private String encoding = UTF8;

    private static final String strictKeyValueSeparators = "=:";

    private static final String whiteSpaceChars = " \t\r\n\f";

    private static final String keyValueSeparators =
            strictKeyValueSeparators + whiteSpaceChars;

    private boolean allowMultipleValues = false;

    /**
     * Properties class extension with customizable encoding support.
     * This is useful for Unicode encodings like UTF-8, which is not
     * compatible with Java's default Properties encoding of 8859-1.
     */
    public BlojsomProperties() {
        super();
    }

    /**
     * Properties class extension with customizable encoding support.
     * This is useful for Unicode encodings like UTF-8, which is not
     * compatible with Java's default Properties encoding of 8859-1.
     * <p></p>
     * If <code>allowMultipleValues</code> is <code>true</code>, then this class
     * will allow multiple values for a single key.  They will be stored under the
     * same key in a {@link List}.
     *
     * @param allowMultipleValues If multiple keys are allowed
     */
    public BlojsomProperties(boolean allowMultipleValues) {
        super();
        this.allowMultipleValues = allowMultipleValues;
    }

    /**
     * Properties class extension with customizable encoding support.
     * This is useful for Unicode encodings like UTF-8, which is not
     * compatible with Java's default Properties encoding of 8859-1.
     *
     * @param defaults Default properties to initialize the Properties object
     */
    public BlojsomProperties(Properties defaults) {
        super(defaults);
    }

    /**
     * Properties class extension with customizable encoding support.
     * This is useful for Unicode encodings like UTF-8, which is not
     * compatible with Java's default Properties encoding of 8859-1.
     *
     * @param encoding Character encoding to use when reading and writing properties
     */
    public BlojsomProperties(String encoding) {
        super();
        this.encoding = encoding;
    }

    /**
     * Set whether or not multiple values should be allowed.  If <code>allowMultipleValues</code> is
     * <code>true</code>, then this class will allow multiple values for a single key.
     * They will be stored under the same key in a {@link List}.
     *
     * @param allowMultipleValues If multiple values should be allowed.
     * @since blojsom 2.18
     */
    public void setAllowMultipleValues(boolean allowMultipleValues) {
        this.allowMultipleValues = allowMultipleValues;
    }

    /**
     * Set the encoding used to read and write the properties file
     *
     * @param encoding File encoding for reading and writing of the properties file
     * @since blojsom 2.18
     */
    public void setEncoding(String encoding) {
        this.encoding = encoding;
    }

    /**
     * Write the properties to disk
     *
     * @param out    Output stream to write to
     * @param header Header to write at the top of the properties file
     * @throws IOException If there is an error writing the properties
     */
    public void store(OutputStream out, String header) throws IOException {

        BufferedWriter writer;

        // If no encoding has been set, use default encoding
        if (encoding == null) {
            writer = new BufferedWriter(new OutputStreamWriter(out));
        } else {
            writer = new BufferedWriter(new OutputStreamWriter(out, encoding));
        }
        if (header != null) {
            writer.write("#" + header);
            writer.newLine();
        }
        writer.write("#" + new Date().toString());
        writer.newLine();
        for (Enumeration e = keys(); e.hasMoreElements();) {
            String key = e.nextElement().toString();
            Object value = get(key);
            key = BlojsomUtils.replace(key, " ", "\\ ");
            if (value != null && value instanceof List && allowMultipleValues) {
                List values = (List) value;
                for (int i = 0; i < values.size(); i++) {
                    value = values.get(i);
                    writer.write(key + "=" + value);
                    writer.newLine();
                }
            } else {
                value = (value != null) ? value.toString() : "";
                writer.write(key + "=" + value);
                writer.newLine();
            }
        }
        writer.flush();
    }

    /**
     * Load the properties from disk
     *
     * @param in Input stream from which to read the properties
     * @throws IOException If there is an error reading the properties
     */
    public void load(InputStream in) throws IOException {
        if (in == null) {
            return;
        }

        BufferedReader reader;
        if (encoding == null) {
            reader = new BufferedReader(new InputStreamReader(in));
        } else {
            reader = new BufferedReader(new InputStreamReader(in, encoding));
        }

        while (true) {
            String line = reader.readLine();
            if (line == null)
                return;
            if (line.length() > 0) {
                if (line.charAt(0) != '#') {
                    int len = line.length();
                    int keyIndex;
                    for (keyIndex = 0; keyIndex < len; keyIndex++) {
                        if (line.charAt(keyIndex) == '\\')
                            keyIndex += 2;
                        else if (whiteSpaceChars.indexOf(line.charAt(keyIndex)) == -1)
                            break;
                    }

                    int separatorIndex;
                    for (separatorIndex = keyIndex; separatorIndex < len; separatorIndex++) {
                        char currentChar = line.charAt(separatorIndex);
                        if (currentChar == '\\')
                            separatorIndex++;
                        else if (keyValueSeparators.indexOf(currentChar) != -1)
                            break;
                    }

                    int valueIndex;
                    for (valueIndex = separatorIndex; valueIndex < len; valueIndex++)
                        if (whiteSpaceChars.indexOf(line.charAt(valueIndex)) == -1)
                            break;

                    if (valueIndex < len)
                        if (strictKeyValueSeparators.indexOf(line.charAt(valueIndex)) != -1)
                            valueIndex++;

                    while (valueIndex < len) {
                        if (whiteSpaceChars.indexOf(line.charAt(valueIndex)) == -1)
                            break;
                        valueIndex++;
                    }

                    String key;
                    String value;

                    key = line.substring(keyIndex, separatorIndex);
                    key = BlojsomUtils.replace(key, "\\", "");
                    value = (separatorIndex < len) ? line.substring(valueIndex, len) : "";

                    List values;
                    if (containsKey(key) && allowMultipleValues) {
                        Object previousValue = get(key);

                        if (previousValue instanceof List) {
                            values = (List) previousValue;
                            values.add(value);
                        } else {
                            values = new ArrayList(1);
                            values.add(previousValue);
                            values.add(value);
                        }

                        put(key, values);
                    } else {
                        put(key, value);
                    }
                }
            }
        }
    }

    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns
     * <code>null</code> if the property is not found.
     *
     * @param key the property key.
     * @return the value in this property list with the specified key value.
     * @see #setProperty
     * @see #defaults
     */
    public String getProperty(String key) {
        if (allowMultipleValues) {
            if (containsKey(key)) {
                Object value = get(key);
                if (value == null) {
                    return null;
                }

                if (value instanceof List) {
                    return BlojsomUtils.listToCSV((List) value);
                }

                return value.toString();
            }
        }

        return super.getProperty(key);
    }

    /**
     * Searches for the property with the specified key in this property list.
     * If the key is not found in this property list, the default property list,
     * and its defaults, recursively, are then checked. The method returns the
     * default value argument if the property is not found.
     *
     * @param key          the hashtable key.
     * @param defaultValue a default value.
     * @return the value in this property list with the specified key value.
     * @see #setProperty
     * @see #defaults
     */
    public String getProperty(String key, String defaultValue) {
        if (allowMultipleValues) {
            if (containsKey(key)) {
                Object value = get(key);
                if (value == null) {
                    return null;
                }

                if (value instanceof List) {
                    return BlojsomUtils.listToCSV((List) value);
                }

                return value.toString();
            }
        }

        return super.getProperty(key, defaultValue);
    }
}

