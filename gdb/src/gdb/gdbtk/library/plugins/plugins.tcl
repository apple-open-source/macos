# Add your window to the PlugIn menu here
# Don't forget to add your packet as well

if {1} {  #test here if your target is configured
  # Uncomment this when the PlugIn class is ready
  #package require LIBGDB 1.0
  package require RHABOUT 1.0
  $Menu add command Other "About Red Hat" \
    {ManagedWin::open RHAbout} \
    -underline 0
  # To activate the PlugIn sample, uncomment the next line
  set plugins_available 1
}
