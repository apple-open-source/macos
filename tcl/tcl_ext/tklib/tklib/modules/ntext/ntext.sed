# apply to text.tcl as the first step in comparing with ntext.tcl
s/bind[[:space:]]\+Text[[:space:]]/bind Ntext /g
s/tk_textPaste/ntext::new_textPaste/g
s/tk_textCut/ntext::new_textCut/g
s/tk::TextInsert/ntext::TextInsert/g
s/tk::TextScrollPages/ntext::TextScrollPages/g
s/tk::TextSelectTo/ntext::TextSelectTo/g
s/tk::TextPasteSelection/ntext::TextPasteSelection/g
s/tk::TextButton1/ntext::TextButton1/g
s/tk::TextAutoScan/ntext::TextAutoScan/g
s/tk::TextTranspose/ntext::TextTranspose/g
s/tcl_wordBreakAfter/ntext::new_wordBreakAfter/g
s/tcl_wordBreakBefore/ntext::new_wordBreakBefore/g
s/tcl_endOfWord/ntext::new_endOfWord/g
s/tcl_startOfNextWord/ntext::new_startOfNextWord/g
s/tcl_startOfPreviousWord/ntext::new_startOfPreviousWord/g
s/tk::TextNextWord/ntext::TextNextWord/g
