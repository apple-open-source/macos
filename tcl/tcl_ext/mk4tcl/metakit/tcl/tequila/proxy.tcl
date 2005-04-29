# test the proxy interface to tequila
# assume tequilas is running on localhost

source tequila.tcl

tequila::open localhost 20458

tequila::proxy ;# this defines all mk::* procs as proxy calls
				# datafile 'tqs' is open and ready for use

mk::view layout tqs.people {name country}

mk::row append tqs.people name jc country nl
mk::row append tqs.people name nb country gr

foreach i [mk::select tqs.people -sort name] {
	puts [mk::get tqs.people!$i]
}

