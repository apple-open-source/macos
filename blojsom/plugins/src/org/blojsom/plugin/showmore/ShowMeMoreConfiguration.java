/**
 * Copyright (c) 2003-2005, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005 by Mark Lussier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
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
package org.blojsom.plugin.showmore;

/**
 * Internal class to hold Show Me More plugin configuration properties
 *
 * @author David Czarnecki
 * @version $Id: ShowMeMoreConfiguration.java,v 1.1.2.1 2005/07/21 04:30:40 johnan Exp $
 * @since blojsom 2.20
 */
public class ShowMeMoreConfiguration {

    private int _cutoff;
    private String _textCutoff;
    private String _moreText;
    private String _textCutoffStart;
    private String _textCutoffEnd;

    /**
     * Default constructor
     */
    public ShowMeMoreConfiguration() {
        _cutoff = ShowMeMorePlugin.ENTRY_TEXT_CUTOFF_DEFAULT;
        _moreText = "More ...";
        _textCutoff = "<more/>";
        _textCutoffStart = "<cut>";
        _textCutoffEnd = "</cut";
    }

    /**
     * Default constructor
     *
     * @param cutoff          Cutoff length
     * @param textCutoff      Cutoff string
     * @param moreText        Text to insert when making a cut
     * @param textCutoffStart Start tag for cutting parts of entries
     * @param textCutoffEnd   End tag for cutting parts of entries
     */
    public ShowMeMoreConfiguration(int cutoff, String textCutoff, String moreText, String textCutoffStart, String textCutoffEnd) {
        _cutoff = cutoff;
        _textCutoff = textCutoff;
        _moreText = moreText;
        _textCutoffStart = textCutoffStart;
        _textCutoffEnd = textCutoffEnd;
    }

    /**
     * Cutoff length
     *
     * @return Cutoff length
     */
    public int getCutoff() {
        return _cutoff;
    }

    /**
     * Cutoff string
     *
     * @return Cutoff string
     */
    public String getTextCutoff() {
        return _textCutoff;
    }

    /**
     * Text to insert when making a cut
     *
     * @return Text to insert when making a cut
     */
    public String getMoreText() {
        return _moreText;
    }

    /**
     * Start tag for cutting parts of entries
     *
     * @return Start tag for cutting parts of entries
     */
    public String getTextCutoffStart() {
        return _textCutoffStart;
    }

    /**
     * End tag for cutting parts of entries
     *
     * @return End tag for cutting parts of entries
     */
    public String getTextCutoffEnd() {
        return _textCutoffEnd;
    }
}
