section Fiber $ColSecTag $y0 $z0 -GJ $GJ {
	for {set i 1} {$i<41} {incr i 1} {
    	fiber [expr -30.75+1.5*$i] [expr -17.21+0.75] 2.25 $matID;
    }
    for {set i 1} {$i<41} {incr i 1} {
    	fiber [expr -30.75+1.5*$i] [expr -17.21+2.25] 2.25 $matID;
    }
    for {set i 1} {$i<41} {incr i 1} {
    	fiber [expr -30.75+1.5*$i] [expr -17.21+3.75] 2.25 $matID;
    }
    for {set i 1} {$i<41} {incr i 1} {
    	fiber [expr -30.75+1.5*$i] [expr -17.21+5.25] 2.25 $matID;
    }


    for {set i 1} {$i<37} {incr i 1} {
    	fiber [expr -2.25] [expr -11.21-0.75+1.5*$i] 2.25 $matID;
    }
    for {set i 1} {$i<37} {incr i 1} {
    	fiber [expr -0.75] [expr -11.21-0.75+1.5*$i] 2.25 $matID;
    }
    for {set i 1} {$i<37} {incr i 1} {
    	fiber [expr 0.75] [expr -11.21-0.75+1.5*$i] 2.25 $matID;
    }
    for {set i 1} {$i<37} {incr i 1} {
    	fiber [expr 2.25] [expr -11.21-0.75+1.5*$i] 2.25 $matID;
    }

}