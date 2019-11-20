# --------------------------------------------------------------------------------------------------
# 3D Steel L-section beam subjected to compressive load on shear center
# Xinlong Du, 9/25/2019
# dispBeamColumn element for Asymmetric sections
#
# SET UP ----------------------------------------------------------------------------
wipe;				# clear memory of all past model definitions
model BasicBuilder -ndm 3 -ndf 6;	# Define the model builder, ndm=#dimension, ndf=#dofs
set dataDir Data;			# set up name of data directory
file mkdir $dataDir; 			# create data directory
#source LibUnits.tcl;			# define units
source DisplayPlane.tcl;		# procedure for displaying a plane in model
source DisplayModel3D.tcl;		# procedure for displaying 3D perspectives of model
# define GEOMETRY ------------------------------------------------------------------
#Nodes, NodeNumber, xCoord, yCoord, zCoord
for {set i 1} {$i<32} {incr i 1} {
	node $i [expr -60.0+60.0*$i] 0 0;
}
# ------ define boundary conditions
# NodeID,dispX,dispY,dispZ,rotX,RotY,RotZ   
set StartNode 1;
set MiddleNode 16;
set EndNode 31;
fix $StartNode 1 1 1 1 0 0;
fix $EndNode 0 1 1 1 0 0;
# Define  SECTIONS -------------------------------------------------------------
# define section tags:
set ColSecTag 1
	# define MATERIAL properties 
	set Es 70000;		# Steel Young's Modulus
	set nu 0.33;
	set Gs [expr $Es/(2*(1+$nu))];  # Torsional stiffness Modulus
	set matID 1;
	uniaxialMaterial Elastic $matID $Es;
	# ELEMENT properties
	# beam-column sections: L5x3.5x5/16
	set J 8424.0;
	set GJ [expr $Gs*$J];
	set y0 0.0;
	set z0 -14.21;
	
source TSec.tcl
	# assign torsional Stiffness for 3D Model
	#uniaxialMaterial Elastic $SecTagTorsion $GJ
	#section Aggregator $ColSecTag $SecTagTorsion T -section $ColSecTagFiber
# define ELEMENTS-----------------------------------------------------------------------------------------------
# set up geometric transformations of element
set IDColTransf 1; # all members
set ColTransfType Corotational;		# options for columns: Linear PDelta Corotational 
geomTransf $ColTransfType  $IDColTransf 0 0 1;	#define geometric transformation: performs a corotational geometric transformation
# Define Beam-Column Elements
set numIntgrPts 5;	# number of Gauss integration points for nonlinear curvature distribution
for {set i 1} {$i<$EndNode} {incr i 1} {
set elemID $i
set nodeI $i
set nodeJ [expr $i+1]
element dispBeamColumn $elemID $nodeI $nodeJ $numIntgrPts $ColSecTag $IDColTransf $y0 $z0;	
} 

# Define RECORDERS -------------------------------------------------------------
recorder Node -file $dataDir/McrLTBDispEndL1800DB30.out -time -node $EndNode -dof 1 2 3 4 5 6 disp;			# displacements of end node
recorder Node -file $dataDir/McrLTBDispMidL1800DB30.out -time -node $MiddleNode -dof 1 2 3 4 5 6 disp;			# displacements of middle node
#recorder Node -file $dataDir/CantileverReac.out -time -node $StartNode -dof 1 2 3 4 5 6 reaction;		# support reaction

# Define DISPLAY -------------------------------------------------------------
DisplayModel3D DeformedShape;	 # options: DeformedShape NodeNumbers ModeShape

# define initial Moment
#-------------------------------------------------------------
set F 50.0;
pattern Plain 1 Linear {
  # NodeID, Fx, Fy, Fz, Mx, My, Mz
  load $MiddleNode   0 $F 0 0 0 0;
  }
constraints Plain;  # Constraint handler -how it handles boundary conditions
numberer Plain;	    # Renumbers DoF to minimize band-width (optimization)
system BandGeneral; # System of equations solver
#test NormUnbalance 1.0e-10 20 1; # tol maxNumIter printFlag
test NormDispIncr 1.0e-8 50 0;
algorithm Newton;# use Newton's solution algorithm: updates tangent stiffness at every iteration
integrator LoadControl 0.1;
analysis Static; 
analyze 10; 

loadConst -time 0.0; # maintains the load constant for the reminder of the analysis and resets the current time to 0
 
# define second stage main Load (Axial force at the two ends)
#-------------------------------------------------------------
set M 1000000.0 
pattern Plain 2 Linear {
  # NodeID, Fx, Fy, Fz, Mx, My, Mz
  load $StartNode 0 0 0 0 -$M 0;
  load $EndNode 0 0 0 0 $M 0; 
 }

# define ANALYSIS PARAMETERS
#------------------------------------------------------------------------------------
constraints Plain; # how it handles boundary conditions
numberer Plain;	   # renumber dof's to minimize band-width 
system BandGeneral;# how to store and solve the system of equations in the analysis
test NormDispIncr 1.0e-08 1000; # determine if convergence has been achieved at the end of an iteration step
#algorithm NewtonLineSearch;# use Newton's solution algorithm: updates tangent stiffness at every iteration
algorithm Newton;
integrator LoadControl 0.005;
#set Dincr 0.005; #-0.00002
#integrator ArcLength 0.05 1.0; #arclength alpha
                                #Node,  dof, 1st incr, Jd, min,   max
#integrator DisplacementControl $EndNode 3   $Dincr     1  $Dincr 0.005;
analysis Static	;# define type of analysis static or transient
analyze 2000;
puts "Finished"
#--------------------------------------------------------------------------------
#set finishTime [clock clicks -milliseconds];
#puts "Time taken: [expr ($finishTime-$startTime)/1000] sec"
#set systemTime [clock seconds] 
#puts "Finished Analysis: [clock format $systemTime -format "%d-%b-%Y %H:%M:%S"]"
