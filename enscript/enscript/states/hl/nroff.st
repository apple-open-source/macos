/**
 * Name: nroff
 * Description: Manual pages formatted with the nroff program.
 * Author: Markku Rossi <mtr@iki.fi>
 */

state nroff_italic extends Highlight
{
  /_\010(.)/ {
    language_print ($1);
  }
  /([^_])\010.\010.\010./ {
    bold (true);
    language_print ($1);
    call (nroff_bold);
    bold (false);
    italic (true);
  }
  /.|\n/ {
    italic (false);
    language_print ($0);
    return;
  }
}

state nroff_bold extends Highlight
{
  /([^_])\010.\010.\010./ {
    language_print ($1);
  }
  /_\010(.)/ {
    italic (true);
    language_print ($1);
    call (nroff_italic);
    italic (false);
    bold (true);
  }
  /.|\n/ {
    bold (false);
    language_print ($0);
    return;
  }
}

state nroff extends HighlightEntry
{
  /_\010(.)/ {
    italic (true);
    language_print ($1);
    call (nroff_italic);
  }
  /([^_])\010.\010.\010./ {
    bold (true);
    language_print ($1);
    call (nroff_bold);
  }
}


/*
Local variables:
mode: c
End:
*/
