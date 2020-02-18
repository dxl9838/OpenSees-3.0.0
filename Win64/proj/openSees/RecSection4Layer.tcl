section Fiber $ColSecTag $y0 $z0 -GJ $GJ {
    for {set i 1} {$i<51} {incr i 1} {
    	fiber [expr -15.3+0.6*$i] -0.225 0.09 $matID;
    }
    for {set i 1} {$i<51} {incr i 1} {
    	fiber [expr -15.3+0.6*$i] -0.075 0.09 $matID;
    }
    for {set i 1} {$i<51} {incr i 1} {
    	fiber [expr -15.3+0.6*$i] 0.075 0.09 $matID;
    }
    for {set i 1} {$i<51} {incr i 1} {
    	fiber [expr -15.3+0.6*$i] 0.225 0.09 $matID;
    }
     
}