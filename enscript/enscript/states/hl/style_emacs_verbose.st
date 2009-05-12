
state style_emacs_verbose
{
  BEGIN {
    call (style_emacs);
    verbose_highlighting = true;
    return;
  }
}


/*
Local variables:
mode: c
End:
*/
