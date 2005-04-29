# Create $childNum^2 elements

set childNum 50
catch {set childNum $numChildren}
set testNum 10
catch {set testNum $numTests}

time {
    set doc [dom::DOMImplementation create]
    set top [dom::document createElement $doc Test]
    for {set i 0} {$i < $childNum} {incr i} {
	set child [dom::document createElement $top Top]
	for {set j 0} {$j < $childNum} {incr j} {
	    dom::document createElement $child Child
	}
    }
} $testNum

