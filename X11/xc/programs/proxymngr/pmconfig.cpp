! $Xorg: pmconfig.cpp,v 1.3 2000/08/17 19:54:00 cpqbld Exp $
! proxy manager config file
!
! Each line has the format:
!    <serviceName> managed <startCommand>
!        or
!    <serviceName> unmanaged <proxyAddress>
!
lbx managed LBXPROXY
!
! substitute site-specific info
!xfwp unmanaged firewall:4444
