/**
 * Copyright (c) 2003-2004 , David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2004  by Mark Lussier
 * BlojsomProperties by Jorg Prante
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

import java.util.Properties;
import java.util.Date;
import java.util.Enumeration;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.IOException;

/**
 * BlojsomProperties
 *
 * Saving properties in non-ISO encodings, based on java.util.Properties
 *
 * @author David Czarnecki
 * @author Jorg Prante
 * @since blojsom 2.01
 * @version $Id: BlojsomProperties.java,v 1.2 2004/08/27 01:13:56 whitmore Exp $
 */
public class BlojsomProperties extends Properties implements BlojsomConstants {

    private String encoding;

    private static final String strictKeyValueSeparators = "=:";

    private static final String whiteSpaceChars = " \t\r\n\f";

    private static final String keyValueSeparators =
            strictKeyValueSeparators + whiteSpaceChars;

    /**
     * Properties class extension with customizable encoding support.
     * This is useful for Unicode encodings like UTF-8, which is not
     * compatible with Java's default Properties encoding of 8859-1.
     */
    public BlojsomProperties() {
        super();
        this.encoding = UTF8;
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
        this.encoding = UTF8;
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
     * Write the properties to disk
     *
     * @param out Output stream to write to
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
            writer.write(key + "=" + get(key).toString());
            writer.newLine();
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
                        if (whiteSpaceChars.indexOf(line.charAt(keyIndex)) == -1)
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
                    put(line.substring(keyIndex, separatorIndex),
                            (separatorIndex < len) ? line.substring(valueIndex, len) : "");
                }
            }
        }
    }
}

