/**
 * Name: outline
 * Description: Outline text.
 * Author: Markku Rossi <mtr@iki.fi>
 */

state outline extends HighlightEntry
{
  /* Titles. */
  /^\*+[ \t]+.+/ {
    bold(true);
    language_print($0);
    bold(false);
  }
}


/*
Local variables:
mode: c
End:
*/
