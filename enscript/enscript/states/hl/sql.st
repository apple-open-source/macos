/**
 * Name: sql
 * Description: Sybase 11 SQL.
 * Author: Chris Jack <chris_jack@msn.com>
 */

state sql_comment extends Highlight
{
  /\\\/\*/ {
    language_print ($0);
    call (sql_comment);
  }
  /\*\\\// {
    language_print ($0);
    return;
  }
}

state sql_string extends Highlight
{
  /\\\\./ {
    language_print ($0);
  }
  /[\']/ {
    language_print ($0);
    return;
  }
}

state sql extends HighlightEntry
{
  /* Comments. */
  /\/\*/ {
    comment_face (true);
    language_print ($0);
    call (sql_comment);
    comment_face (false);
  }

  /* String constants. */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* Character constants. */
  /[\']/ {
    string_face (true);
    language_print ($0);
    call (sql_string);
    string_face (false);
  }

  /* Keywords.
   */
  /\b(\
[Aa][Bb][Ss]|\
[Aa][Cc][Oo][Ss]|\
[Aa][Dd][Dd]|\
[Aa][Ll][Ll]|\
[Aa][Ll][Ll][Oo][Ww]_[Dd][Uu][Pp]_[Rr][Oo][Ww]|\
[Aa][Ll][Tt][Ee][Rr]|\
[Aa][Nn][Dd]|\
[Aa][Nn][Ss][Ii][Nn][Uu][Ll][Ll]|\
[Aa][Nn][Ss][Ii]_[Pp][Ee][Rr][Mm][Ii][Ss][Ss][Ii][Oo][Nn][Ss]|\
[Aa][Nn][Yy]|\
[Aa][Rr][Ii][Tt][Hh]_[Oo][Vv][Ee][Rr][Ff][Ll][Oo][Ww]|\
[Aa][Rr][Ii][Tt][Hh][Aa][Bb][Oo][Rr][Tt]|\
[Aa][Rr][Ii][Tt][Hh][Ii][Gg][Nn][Oo][Rr][Ee]|\
[Aa][Ss]|\
[Aa][Ss][Cc]|\
[Aa][Ss][Cc][Ii][Ii]|\
[Aa][Ss][Ii][Nn]|\
[Aa][Tt]|\
[Aa][Tt][Aa][Nn]|\
[Aa][Tt][Nn]2|\
[Aa][Uu][Tt][Hh][Oo][Rr][Ii][Zz][Aa][Tt][Ii][Oo][Nn]|\
[Aa][Uu][Tt][Oo]|\
[Aa][Vv][Gg]|\
[Bb][Ee][Gg][Ii][Nn]|\
[Bb][Ee][Tt][Ww][Ee][Ee][Nn]|\
[Bb][Ii][Nn][Aa][Rr][Yy]|\
[Bb][Rr][Ee][Aa][Kk]|\
[Bb][Rr][Oo][Ww][Ss][Ee]|\
[Bb][Uu][Ll][Kk]|\
[Bb][Uu][Ll][Kk][Cc][Oo][Pp][Yy]|\
[Bb][Yy]|\
[Cc][Aa][Ss][Cc][Aa][Dd][Ee]|\
[Cc][Ee][Ii][Ll][Ii][Nn][Gg]|\
[Cc][Hh][Aa][Ii][Nn][Ee][Dd]|\
[Cc][Hh][Aa][Rr]|\
[Cc][Hh][Aa][Rr]_[Cc][Oo][Nn][Vv][Ee][Rr][Tt]|\
[Cc][Hh][Aa][Rr]_[Ll][Ee][Nn][Gg][Tt][Hh]|\
[Cc][Hh][Aa][Rr][Aa][Cc][Tt][Ee][Rr]|\
[Cc][Hh][Aa][Rr][Ii][Nn][Dd][Ee][Xx]|\
[Cc][Hh][Ee][Cc][Kk]|\
[Cc][Hh][Ee][Cc][Kk][Pp][Oo][Ii][Nn][Tt]|\
[Cc][Ll][Oo][Ss][Ee]|\
[Cc][Ll][Uu][Ss][Tt][Ee][Rr][Ee][Dd]|\
[Cc][Oo][Ll]_[Ll][Ee][Nn][Gg][Tt][Hh]|\
[Cc][Oo][Ll]_[Nn][Aa][Mm][Ee]|\
[Cc][Oo][Mm][Mm][Ii][Tt]|\
[Cc][Oo][Mm][Pp][Uu][Tt][Ee]|\
[Cc][Oo][Nn][Ff][Ii][Rr][Mm]|\
[Cc][Oo][Nn][Ss][Tt][Rr][Aa][Ii][Nn][Tt]|\
[Cc][Oo][Nn][Tt][Ii][Nn][Uu][Ee]|\
[Cc][Oo][Nn][Tt][Rr][Oo][Ll][Rr][Oo][Ww]|\
[Cc][Oo][Nn][Vv][Ee][Rr][Tt]|\
[Cc][Oo][Ss]|\
[Cc][Oo][Tt]|\
[Cc][Oo][Uu][Nn][Tt]|\
[Cc][Rr][Ee][Aa][Tt][Ee]|\
[Cc][Uu][Rr][Rr][Ee][Nn][Tt]|\
[Cc][Uu][Rr][Ss][Oo][Rr]|\
[Cc][Uu][Rr][Uu][Nn][Rr][Ee][Ss][Ee][Rr][Vv][Ee][Dd][Pp][Gg][Ss]|\
[Dd][Aa][Tt][Aa]_[Pp][Gg][Ss]|\
[Dd][Aa][Tt][Aa][Bb][Aa][Ss][Ee]|\
[Dd][Aa][Tt][Aa][Ll][Ee][Nn][Gg][Tt][Hh]|\
[Dd][Aa][Tt][Ee][Aa][Dd][Dd]|\
[Dd][Aa][Tt][Ee][Dd][Ii][Ff][Ff]|\
[Dd][Aa][Tt][Ee][Ff][Ii][Rr][Ss][Tt]|\
[Dd][Aa][Tt][Ee][Ff][Oo][Rr][Mm][Aa][Tt]|\
[Dd][Aa][Tt][Ee][Nn][Aa][Mm][Ee]|\
[Dd][Aa][Tt][Ee][Pp][Aa][Rr][Tt]|\
[Dd][Aa][Tt][Ee][Tt][Ii][Mm][Ee]|\
[Dd][Bb]_[Ii][Dd]|\
[Dd][Bb]_[Nn][Aa][Mm][Ee]|\
[Dd][Bb][Cc][Cc]|\
[Dd][Ee][Aa][Ll][Ll][Oo][Cc][Aa][Tt][Ee]|\
[Dd][Ee][Cc][Ii][Mm][Aa][Ll]|\
[Dd][Ee][Cc][Ll][Aa][Rr][Ee]|\
[Dd][Ee][Ff][Aa][Uu][Ll][Tt]|\
[Dd][Ee][Ff][Ii][Nn][Ee]|\
[Dd][Ee][Gg][Rr][Ee][Ee][Ss]|\
[Dd][Ee][Ll][Ee][Tt][Ee]|\
[Dd][Ee][Ss][Cc]|\
[Dd][Ii][Ff][Ff][Ee][Rr][Ee][Nn][Cc][Ee]|\
[Dd][Ii][Ss][Kk]|\
[Dd][Ii][Ss][Tt][Ii][Nn][Cc][Tt]|\
[Dd][Oo][Uu][Bb][Ll][Ee]|\
[Dd][Rr][Oo][Pp]|\
[Dd][Uu][Mm][Mm][Yy]|\
[Dd][Uu][Mm][Pp]|\
[Ee][Ll][Ss][Ee]|\
[Ee][Nn][Dd]|\
[Ee][Nn][Dd][Tt][Rr][Aa][Nn]|\
[Ee][Rr][Rr][Ll][Vv][Ll]|\
[Ee][Rr][Rr][Oo][Rr]|\
[Ee][Rr][Rr][Oo][Rr][Dd][Aa][Tt][Aa]|\
[Ee][Rr][Rr][Oo][Rr][Ee][Xx][Ii][Tt]|\
[Ee][Ss][Cc][Aa][Pp][Ee]|\
[Ee][Xx][Cc][Ee][Pp][Tt]|\
[Ee][Xx][Ee][Cc][Uu][Tt][Ee]|\
[Ee][Xx][Ii][Ss][Tt][Ss]|\
[Ee][Xx][Ii][Tt]|\
[Ee][Xx][Pp]|\
[Ff][Ee][Tt][Cc][Hh]|\
[Ff][Ii][Ll][Ll][Ff][Aa][Cc][Tt][Oo][Rr]|\
[Ff][Ii][Pp][Ss][Ff][Ll][Aa][Gg][Gg][Ee][Rr]|\
[Ff][Ll][Oo][Aa][Tt]|\
[Ff][Ll][Oo][Oo][Rr]|\
[Ff][Ll][Uu][Ss][Hh][Mm][Ee][Ss][Ss][Aa][Gg][Ee]|\
[Ff][Oo][Rr]|\
[Ff][Oo][Rr][Ee][Ii][Gg][Nn]|\
[Ff][Rr][Oo][Mm]|\
[Gg][Ee][Tt][Dd][Aa][Tt][Ee]|\
[Gg][Oo][Tt][Oo]|\
[Gg][Rr][Aa][Nn][Tt]|\
[Gg][Rr][Oo][Uu][Pp]|\
[Hh][Aa][Vv][Ii][Nn][Gg]|\
[Hh][Ee][Xx][Tt][Oo][Ii][Nn][Tt]|\
[Hh][Oo][Ll][Dd][Ll][Oo][Cc][Kk]|\
[Hh][Oo][Ss][Tt]_[Nn][Aa][Mm][Ee]|\
[Ii][Dd][Ee][Nn][Tt][Ii][Tt][Yy]|\
[Ii][Dd][Ee][Nn][Tt][Ii][Tt][Yy]_[Ii][Nn][Ss][Ee][Rr][Tt]|\
[Ii][Ff]|\
[Ii][Gg][Nn][Oo][Rr][Ee]_[Dd][Uu][Pp]_[Kk][Ee][Yy]|\
[Ii][Gg][Nn][Oo][Rr][Ee]_[Dd][Uu][Pp]_[Rr][Oo][Ww]|\
[Ii][Mm][Aa][Gg][Ee]|\
[Ii][Nn]|\
[Ii][Nn][Dd][Ee][Xx]|\
[Ii][Nn][Dd][Ee][Xx]_[Cc][Oo][Ll]|\
[Ii][Nn][Ss][Ee][Rr][Tt]|\
[Ii][Nn][Tt]|\
[Ii][Nn][Tt][Ee][Gg][Ee][Rr]|\
[Ii][Nn][Tt][Ee][Rr][Ss][Ee][Cc][Tt]|\
[Ii][Nn][Tt][Oo]|\
[Ii][Nn][Tt][Tt][Oo][Hh][Ee][Xx]|\
[Ii][Oo]|\
[Ii][Ss]|\
[Ii][Ss][Nn][Uu][Ll][Ll]|\
[Ii][Ss][Oo][Ll][Aa][Tt][Ii][Oo][Nn]|\
[Kk][Ee][Yy]|\
[Kk][Ii][Ll][Ll]|\
[Ll][Aa][Nn][Gg][Uu][Aa][Gg][Ee]|\
[Ll][Cc][Tt]_[Aa][Dd][Mm][Ii][Nn]|\
[Ll][Ee][Vv][Ee][Ll]|\
[Ll][Ii][Kk][Ee]|\
[Ll][Ii][Nn][Ee][Nn][Oo]|\
[Ll][Oo][Aa][Dd]|\
[Ll][Oo][Gg]|\
[Ll][Oo][Gg]10|\
[Ll][Oo][Ww][Ee][Rr]|\
[Ll][Rr][Uu]|\
[Ll][Tt][Rr][Ii][Mm]|\
[Mm][Aa][Xx]|\
[Mm][Ii][Nn]|\
[Mm][Ii][Rr][Rr][Oo][Rr]|\
[Mm][Ii][Rr][Rr][Oo][Rr][Ee][Xx][Ii][Tt]|\
[Mm][Oo][Nn][Ee][Yy]|\
[Mm][Rr][Uu]|\
[Nn][Aa][Tt][Ii][Oo][Nn][Aa][Ll]|\
[Nn][Cc][Hh][Aa][Rr]|\
[Nn][Oo]_[Ee][Rr][Rr][Oo][Rr]|\
[Nn][Oo][Cc][Oo][Uu][Nn][Tt]|\
[Nn][Oo][Ee][Xx][Ee][Cc]|\
[Nn][Oo][Hh][Oo][Ll][Dd][Ll][Oo][Cc][Kk]|\
[Nn][Oo][Nn][Cc][Ll][Uu][Ss][Tt][Ee][Rr][Ee][Dd]|\
[Nn][Oo][Tt]|\
[Nn][Uu][Ll][Ll]|\
[Nn][Uu][Mm][Ee][Rr][Ii][Cc]|\
[Nn][Uu][Mm][Ee][Rr][Ii][Cc]_[Tt][Rr][Uu][Nn][Cc][Aa][Tt][Ii][Oo][Nn]|\
[Nn][Vv][Aa][Rr][Cc][Hh][Aa][Rr]|\
[Oo][Bb][Jj][Ee][Cc][Tt]_[Ii][Dd]|\
[Oo][Bb][Jj][Ee][Cc][Tt]_[Nn][Aa][Mm][Ee]|\
[Oo][Ff]|\
[Oo][Ff][Ff]|\
[Oo][Ff][Ff][Ss][Ee][Tt][Ss]|\
[Oo][Nn]|\
[Oo][Nn][Cc][Ee]|\
[Oo][Nn][Ll][Yy]|\
[Oo][Pp][Ee][Nn]|\
[Oo][Pp][Tt][Ii][Oo][Nn]|\
[Oo][Rr]|\
[Oo][Rr][Dd][Ee][Rr]|\
[Oo][Vv][Ee][Rr]|\
[Pp][Aa][Rr][Aa][Mm]|\
[Pp][Aa][Rr][Ss][Ee][Oo][Nn][Ll][Yy]|\
[Pp][Aa][Tt][Ii][Nn][Dd][Ee][Xx]|\
[Pp][Ee][Rr][Mm][Aa][Nn][Ee][Nn][Tt]|\
[Pp][Ii]|\
[Pp][Ll][Aa][Nn]|\
[Pp][Oo][Ww][Ee][Rr]|\
[Pp][Rr][Ee][Cc][Ii][Ss][Ii][Oo][Nn]|\
[Pp][Rr][Ee][Ff][Ee][Tt][Cc][Hh]|\
[Pp][Rr][Ee][Pp][Aa][Rr][Ee]|\
[Pp][Rr][Ii][Mm][Aa][Rr][Yy]|\
[Pp][Rr][Ii][Nn][Tt]|\
[Pp][Rr][Ii][Vv][Ii][Ll][Ee][Gg][Ee][Ss]|\
[Pp][Rr][Oo][Cc]|\
[Pp][Rr][Oo][Cc][Ii][Dd]|\
[Pp][Rr][Oo][Cc]_[Rr][Oo][Ll][Ee]|\
[Pp][Rr][Oo][Cc][Ee][Dd][Uu][Rr][Ee]|\
[Pp][Rr][Oo][Cc][Ee][Ss][Ss][Ee][Xx][Ii][Tt]|\
[Pp][Uu][Bb][Ll][Ii][Cc]|\
[Qq][Uu][Oo][Tt][Ee][Dd]_[Ii][Dd][Ee][Nn][Tt][Ii][Ff][Ii][Ee][Rr]|\
[Rr][Aa][Dd][Ii][Aa][Nn][Ss]|\
[Rr][Aa][Ii][Ss][Ee][Rr][Rr][Oo][Rr]|\
[Rr][Aa][Nn][Dd]|\
[Rr][Ee][Aa][Dd]|\
[Rr][Ee][Aa][Dd][Tt][Ee][Xx][Tt]|\
[Rr][Ee][Aa][Ll]|\
[Rr][Ee][Cc][Oo][Nn][Ff][Ii][Gg][Uu][Rr][Ee]|\
[Rr][Ee][Ff][Ee][Rr][Ee][Nn][Cc][Ee][Ss]|\
[Rr][Ee][Pp][Ll][Aa][Cc][Ee]|\
[Rr][Ee][Pp][Ll][Ii][Cc][Aa][Tt][Ee]|\
[Rr][Ee][Ss][Ee][Rr][Vv][Ee][Dd]_[Pp][Gg][Ss]|\
[Rr][Ee][Ss][Tt][Rr][Ee][Ee]|\
[Rr][Ee][Tt][Uu][Rr][Nn]|\
[Rr][Ee][Vv][Ee][Rr][Ss][Ee]|\
[Rr][Ee][Vv][Oo][Kk][Ee]|\
[Rr][Ii][Gg][Hh][Tt]|\
[Rr][Oo][Ll][Ee]|\
[Rr][Oo][Ll][Ll][Bb][Aa][Cc][Kk]|\
[Rr][Oo][Uu][Nn][Dd]|\
[Rr][Oo][Ww][Cc][Nn][Tt]|\
[Rr][Oo][Ww][Cc][Oo][Uu][Nn][Tt]|\
[Rr][Oo][Ww][Ss]|\
[Rr][Tt][Rr][Ii][Mm]|\
[Rr][Uu][Ll][Ee]|\
[Ss][Aa][Vv][Ee]|\
[Ss][Cc][Hh][Ee][Mm][Aa]|\
[Ss][Ee][Ll][Ee][Cc][Tt]|\
[Ss][Ee][Ll][Ff]_[Rr][Ee][Cc][Uu][Rr][Ss][Ii][Oo][Nn]|\
[Ss][Ee][Tt]|\
[Ss][Ee][Tt][Uu][Ss][Ee][Rr]|\
[Ss][Hh][Aa][Rr][Ee][Dd]|\
[Ss][Hh][Oo][Ww]_[Rr][Oo][Ll][Ee]|\
[Ss][Hh][Oo][Ww][Pp][Ll][Aa][Nn]|\
[Ss][Hh][Uu][Tt][Dd][Oo][Ww][Nn]|\
[Ss][Ii][Gg][Nn]|\
[Ss][Ii][Nn]|\
[Ss][Mm][Aa][Ll][Ll][Dd][Aa][Tt][Ee][Tt][Ii][Mm][Ee]|\
[Ss][Mm][Aa][Ll][Ll][Ii][Nn][Tt]|\
[Ss][Mm][Aa][Ll][Ll][Mm][Oo][Nn][Ee][Yy]|\
[Ss][Oo][Uu][Nn][Dd][Ee][Xx]|\
[Ss][Pp][Aa][Cc][Ee]|\
[Ss][Qq][Rr][Tt]|\
[Ss][Tt][Aa][Tt][Ee][Mm][Ee][Nn][Tt]|\
[Ss][Tt][Aa][Tt][Ii][Ss][Tt][Ii][Cc][Ss]|\
[Ss][Tt][Rr]|\
[Ss][Tt][Rr][Ii][Nn][Gg]_[Rr][Tt][Rr][Uu][Nn][Cc][Aa][Tt][Ii][Oo][Nn]|\
[Ss][Tt][Rr][Ii][Pp][Ee]|\
[Ss][Tt][Uu][Ff][Ff]|\
[Ss][Uu][Bb][Qq][Uu][Ee][Rr][Yy][Cc][Aa][Cc][Hh][Ee]|\
[Ss][Uu][Bb][Ss][Tt][Rr][Ii][Nn][Gg]|\
[Ss][Uu][Mm]|\
[Ss][Uu][Ss][Ee][Rr]_[Ii][Dd]|\
[Ss][Uu][Ss][Ee][Rr]_[Nn][Aa][Mm][Ee]|\
[Ss][Yy][Bb]_[Ii][Dd][Ee][Nn][Tt][Ii][Tt][Yy]|\
[Tt][Aa][Bb][Ll][Ee]|\
[Tt][Aa][Nn]|\
[Tt][Ee][Mm][Pp][Oo][Rr][Aa][Rr][Yy]|\
[Tt][Ee][Rr][Mm][Ii][Nn][Aa][Tt][Ee]|\
[Tt][Ee][Xx][Tt]|\
[Tt][Ee][Xx][Tt][Ss][Ii][Zz][Ee]|\
[Tt][Ii][Mm][Ee]|\
[Tt][Ii][Nn][Yy][Ii][Nn][Tt]|\
[Tt][Oo]|\
[Tt][Rr][Aa][Nn][Ss][Aa][Cc][Tt][Ii][Oo][Nn]|\
[Tt][Rr][Ii][Gg][Gg][Ee][Rr]|\
[Tt][Rr][Uu][Nn][Cc][Aa][Tt][Ee]|\
[Tt][Ss][Ee][Qq][Uu][Aa][Ll]|\
[Uu][Nn][Ii][Oo][Nn]|\
[Uu][Nn][Ii][Qq][Uu][Ee]|\
[Uu][Pp][Dd][Aa][Tt][Ee]|\
[Uu][Pp][Pp][Ee][Rr]|\
[Uu][Ss][Ee]|\
[Uu][Ss][Ee][Dd]_[Pp][Gg][Ss]|\
[Uu][Ss][Ee][Rr]|\
[Uu][Ss][Ee][Rr]_[Ii][Dd]|\
[Uu][Ss][Ee][Rr]_[Nn][Aa][Mm][Ee]|\
[Uu][Ss][Ee][Rr]_[Oo][Pp][Tt][Ii][Oo][Nn]|\
[Uu][Ss][Ii][Nn][Gg]|\
[Vv][Aa][Ll][Ii][Dd]_[Nn][Aa][Mm][Ee]|\
[Vv][Aa][Ll][Ii][Dd]_[Uu][Ss][Ee][Rr]|\
[Vv][Aa][Ll][Uu][Ee][Ss]|\
[Vv][Aa][Rr][Bb][Ii][Nn][Aa][Rr][Yy]|\
[Vv][Aa][Rr][Cc][Hh][Aa][Rr]|\
[Vv][Aa][Rr][Yy][Ii][Nn][Gg]|\
[Vv][Ii][Ee][Ww]|\
[Ww][Aa][Ii][Tt][Ff][Oo][Rr]|\
[Ww][Hh][Ee][Rr][Ee]|\
[Ww][Hh][Ii][Ll][Ee]|\
[Ww][Ii][Tt][Hh]|\
[Ww][Oo][Rr][Kk]|\
[Ww][Rr][Ii][Tt][Ee][Tt][Ee][Xx][Tt]\
)\b/ {
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }
}


/*
Local variables:
mode: c
End:
*/
