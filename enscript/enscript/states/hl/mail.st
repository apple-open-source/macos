/**
 * Name: mail
 * Description: Mail and news articles.
 * Author: Originally by Markku Rossi <mtr@iki.fi>, heavily modified
 * by Matthew Weigel <weigel@pitt.edu>
 */

state mail_body extends Highlight
{
  BEGIN {
    reference_face (false);
  }
  /^[ \t]*>/ {
    comment_face (true);
    language_print ($0);
    call (eat_one_line);
    comment_face (false);
  }
}

state mail_header extends Highlight
{
  BEGIN {
    reference_face (true);
  }

  END {
    comment_face (true);
  }
  /:/ {
    language_print ($0);
    return;
  }
}

state mail extends HighlightEntry
{
  /^[ \t]*$/ {
    /* Move to the mail body. */
    call (mail_body);
  }

  /^/ {
    reference_face (true);
    language_print ($0);
    call (mail_header);
    comment_face (true);
  }
}


/*
Local variables:
mode: c
End:
*/
