cat <<EOF
%define name fetchmail
%define version ${1}
%define release 1
%define builddir \$RPM_BUILD_DIR/%{name}-%{version}
Name:		%{name}
Version:	%{version}
Release:	%{release}
Vendor:		Eric Conspiracy Secret Labs
Packager:	Eric S. Raymond <esr@thyrsus.com>
URL:		http://www.tuxedo.org/~esr/fetchmail
Source:         %{name}-%{version}.tar.gz
Group:		Applications/Mail
Group(pt_BR):   Aplicações/Correio Eletrônico
Copyright:	GPL
Icon:		fetchmail.xpm
Requires:	smtpdaemon
BuildRoot:	/var/tmp/%{name}-%{version}
Summary:	Full-featured POP/IMAP mail retrieval daemon
Summary(fr):    Collecteur (POP/IMAP) de courrier électronique
Summary(de):    Program zum Abholen von E-Mail via POP/IMAP
Summary(pt):    Busca mensagens de um servidor usando POP ou IMAP
Summary(es):    Recolector de correo via POP/IMAP
Summary(pl):    Zdalny demon pocztowy do protoko³ów POP2, POP3, APOP, IMAP
Summary(tr):    POP2, POP3, APOP, IMAP protokolleri ile uzaktan mektup alma yazýlýmý
Summary(da):    Alsidig POP/IMAP post-afhentnings dæmon

%description
Fetchmail is a free, full-featured, robust, and well-documented remote
mail retrieval and forwarding utility intended to be used over
on-demand TCP/IP links (such as SLIP or PPP connections).  It
retrieves mail from remote mail servers and forwards it to your local
(client) machine's delivery system, so it can then be be read by
normal mail user agents such as mutt, elm, pine, (x)emacs/gnus, or mailx.
Comes with an interactive GUI configurator suitable for end-users.

%description -l fr
Fetchmail est un programme qui permet d'aller rechercher du courrier
électronique sur un serveur de mail distant. Fetchmail connait les
protocoles POP (Post Office Protocol), IMAP (Internet Mail Access
Protocol) et délivre le courrier électronique a travers le
serveur SMTP local (habituellement sendmail).

%description -l de
Fetchmail ist ein freies, vollständiges, robustes und
wohldokumentiertes Werkzeug zum Abholen und Weiterreichen von E-Mail,
gedacht zum Gebrauch über temporäre TCP/IP-Verbindungen (wie
z.B. SLIP- oder PPP-Verbindungen).  Es holt E-Mail von (weit)
entfernten Mail-Servern ab und reicht sie an das Auslieferungssystem
der lokalen Client-Maschine weiter, damit sie dann von normalen MUAs
("mail user agents") wie mutt, elm, pine, (x)emacs/gnus oder mailx
gelesen werden können.  Ein interaktiver GUI-Konfigurator auch gut
geeignet zum Gebrauch durch Endbenutzer wird mitgeliefert.

%description -l pt
Fetchmail é um programa que é usado para recuperar mensagens de um
servidor de mail remoto. Ele pode usar Post Office Protocol (POP)
ou IMAP (Internet Mail Access Protocol) para isso, e entrega o mail
através do servidor local SMTP (normalmente sendmail).

%description -l es
Fetchmail es una utilidad gratis, completa, robusta y bien documentada
para la recepción y reenvío de correo pensada para ser usada en
conexiones TCP/IP temporales (como SLIP y PPP). Recibe el correo de
servidores remotos y lo reenvía al sistema de entrega local, siendo de
ese modo posible leerlo con programas como mutt, elm, pine, (x)emacs/gnus
o mailx. Contiene un configurador GUI interactivo pensado para usuarios.

%description -l pl
Fetchmail jest programem do ¶ci±gania poczty ze zdalnych serwerów
pocztowych. Do ¶ci±gania poczty mo¿e on uzywaæ protoko³ów POP (Post Office
Protocol) lub IMAP (Internet Mail Access Protocol). ¦ci±gniêt± pocztê
dostarcza do koñcowych odbiorców poprzez lokalny serwer SMTP.

description -l tr
fetchmail yazýlýmý, POP veya IMAP desteði veren bir sunucuda yer alan
mektuplarýnýzý alýr.

description -l da
Fetchmail er et gratis, robust, alsidigt og vel-dokumenteret værktøj 
til afhentning og videresending af elektronisk post via TCP/IP
baserede opkalds-forbindelser (såsom SLIP eller PPP forbindelser).   
Den henter post fra en ekstern post-server, og videresender den
til din lokale klient-maskines post-system, så den kan læses af
almindelige mail klienter såsom mutt, elm, pine, (x)emacs/gnus,
eller mailx. Der medfølger også et interaktivt GUI-baseret
konfigurations-program, som kan bruges af almindelige brugere.

%package -n fetchmailconf
Summary:        A GUI configurator for generating fetchmail configuration files
Summary(pl):    GUI konfigurator do fetchmaila
Summary(fr):	GUI configurateur pour fetchmail
Summary(es):	Configurador GUI interactivo para fetchmail
Group:          Utilities/System
Requires:       %{name} = %{version}, python

%description -n fetchmailconf
A GUI configurator for generating fetchmail configuration file written in
python

%description -n fetchmailconf -l es
Configurador gráfico para fetchmail escrito en python

%description -n fetchmailconf -l de
Ein interaktiver GUI-Konfigurator für fetchmail in python

%description -n fetchmailconf -l pl
GUI konfigurator do fetchmaila napisany w pythonie.

%prep
%setup -q

%build
CFLAGS="$RPM_OPT_FLAGS" LDFLAGS="-s" # Add  --enable-nls --without-included-gettext for internationalization
export CFLAGS LDFLAGS
./configure -prefix=/usr
make

%install
if [ -d \$RPM_BUILD_ROOT ]; then rm -rf \$RPM_BUILD_ROOT; fi
mkdir -p \$RPM_BUILD_ROOT/{etc/X11/wmconfig,usr/lib/rhs/control-panel}
make install prefix=\$RPM_BUILD_ROOT/usr
cp %{builddir}/rh-config/*.{xpm,init} \$RPM_BUILD_ROOT/usr/lib/rhs/control-panel
cp %{builddir}/fetchmail.man \$RPM_BUILD_ROOT/usr/man/man1/fetchmail.1
gzip -9nf \$RPM_BUILD_ROOT/usr/man/man1/fetchmail.1
cd \$RPM_BUILD_ROOT/usr/man/man1
ln -sf fetchmail.1.gz fetchmailconf.1.gz
rm -rf %{builddir}/contrib/RCS
chmod 644 %{builddir}/contrib/*
cp %{builddir}/rh-config/fetchmailconf.wmconfig \$RPM_BUILD_ROOT/etc/X11/wmconfig/fetchmailconf

%clean
rm -rf \$RPM_BUILD_ROOT

%files
%defattr (644, root, root, 755)
%doc README NEWS NOTES FAQ COPYING FEATURES sample.rcfile contrib
%doc fetchmail-features.html fetchmail-FAQ.html design-notes.html
/usr/lib/rhs/control-panel/fetchmailconf.xpm
/usr/lib/rhs/control-panel/fetchmailconf.init
/etc/X11/wmconfig/fetchmailconf
%defattr (644, root, man)
/usr/man/man1/*.1.gz
%defattr (755, root, root)
/usr/bin/fetchmail
/usr/bin/fetchmailconf
# Add these for internationalization
#/usr/share/locale/es/LC_MESSAGES/fetchmail.mo
#/usr/share/locale/pl/LC_MESSAGES/fetchmail.mo
#/usr/share/locale/pt_BR/LC_MESSAGES/fetchmail.mo
EOF
