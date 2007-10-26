int _gdbcp_fork ()
{
  int status;
  int pid = fork ();

  /* Return the child's pid, so debugger can manage it.  */
  if (pid != 0)
    return pid;
  /* Make the child spin forever.  */
  while (1)
    {
      wait (&status);
      sleep (1);
    }
}
