/* APPLE LOCAL  nested functions */
/* { dg-xfail-if "" { "*-*-darwin*" } { "*" } { "" } } */
/* APPLE LOCAL begin mainline 2005-08-08 */
/* PR 21894 */

void
CheckFile ()
{
  char tagname[10];
  char *a = tagname;

  int validate ()
  {
    return (a == tagname + 4);
  }

  if (a == tagname)
    validate ();
}
/* APPLE LOCAL end mainline 2005-08-08 */
