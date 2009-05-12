/**
 * Name: rfc
 * Description: RFC and Internet Draft reformatter.
 *
 * 		Some RFCs and Internet Drafts are broken in such a way
 * 		that there is no linefeed character separating the
 * 		pages.  These documents are really pain to print
 * 		because you have to know the exact page height.  The
 * 		`rfc' highlighting state fixes these broken documents
 * 		by removing all unnecessary blank lines from the page
 * 		breaks and by separating the pages with a formfeed
 * 		character.  This style also highlights the MUST, MUST
 * 		NOT, REQUIRED SHALL, SHALL NOT, SHOULD, SHOULD NOT,
 * 		RECOMMENDED, MAY, and OPTIONAL keywords, described by
 * 		the RFC 2119.
 *
 * Author: Markku Rossi <mtr@iki.fi>
 */

state rfc_page_separator_garbage extends Highlight
{
  /^[ \t\f\r]*\n/ {
    /* Skip all whitespace lines. */
  }
  /./ {
    /* Print the correct separator line. */
    language_print("\f\n");
    language_print($0);
    return;
  }
}

state rfc extends HighlightEntry
{
  /* Find the page separators and skip all whitespace lines after that. */
  /\[[Pp]age[ \t]+[0-9]+[ \t]*\]\n/ {
    language_print($0);
    call(rfc_page_separator_garbage);
  }
  /* Highlight the keywords, described by the RFC 2119.
     (build-re '(MUST NOT REQUIRED SHALL SHOULD RECOMMENDED MAY OPTIONAL))
   */
  /\b(M(AY|UST)|NOT|OPTIONAL|RE(COMMENDED|QUIRED)|SH(ALL|OULD))\b/ {
    highlight_face(true);
    language_print($0);
    highlight_face(false);
  }
}


/*
Local variables:
mode: c
End:
*/
