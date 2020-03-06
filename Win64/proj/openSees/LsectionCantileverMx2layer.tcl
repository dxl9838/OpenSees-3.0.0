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
node 1	0.00	0	0
node 2  8.89    0   0
node 3	17.78	0	0
node 4	26.67	0	0
node 5	35.56	0	0
node 6	44.45   0	0
node 7	53.34	0	0
node 8	62.23	0	0
node 9	71.12	0	0
node 10	80.01	0	0
node 11	88.9	0	0
node 12	97.79	0	0
node 13	106.68	0	0
node 14	115.57	0	0
node 15	124.46	0	0
node 16	133.35	0	0
node 17	142.24	0	0
node 18	151.13	0	0
node 19	160.02	0	0
node 20	168.91	0	0
node 21	177.8	0	0
# ------ define boundary conditions
# NodeID,dispX,dispY,dispZ,rotX,RotY,RotZ 
fix 1  1 1 1 1 1 1;    
set StartNode 1;
set MiddleNode 11;
set EndNode 21;
# Define  SECTIONS -------------------------------------------------------------
# define section tags:
set ColSecTag 1
#set ColSecTagFiber 4
#set SecTagTorsion 70
	# define MATERIAL properties 
	set Es 89632;		# Steel Young's Modulus
	set nu 0.3;
	set Gs 33445;  # Torsional stiffness Modulus
	set matID 1;
	uniaxialMaterial Elastic $matID $Es;
	# ELEMENT properties
	# beam-column sections: L5x3.5x5/16
	set J 8.33383829;
	set GJ [expr $Gs*$J]
	set y0 -4.98815575;
	set z0 0.00000000;
	
source Lsection2layer.tcl
	#set oC 0.0;
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
element mixedBeamColumn $elemID $nodeI $nodeJ $numIntgrPts $ColSecTag $IDColTransf -shearCenter $y0 $z0;	
} 

# Define RECORDERS -------------------------------------------------------------
recorder Node -file $dataDir/mixedCantileverDisp2layer.out -time -node $EndNode -dof 1 2 3 4 5 6 disp;			# displacements of middle node
recorder Node -file $dataDir/mixedCantileverReac2layer.out -time -node $StartNode -dof 1 2 3 4 5 6 reaction;		# support reaction

# Define DISPLAY -------------------------------------------------------------
DisplayModel3D DeformedShape;	 # options: DeformedShape NodeNumbers ModeShape

# define second stage main Load (Axial force at the two ends)
#------------------------------------------------------------- 
set N 150.0;
pattern Plain 2 Linear {
  # NodeID, Fx, Fy, Fz, Mx, My, Mz
  load $EndNode 0 0 0 -$N 0 0; 
 }

# define ANALYSIS PARAMETERS
#------------------------------------------------------------------------------------
constraints Plain; # how it handles boundary conditions
numberer Plain;	   # renumber dof's to minimize band-width 
system BandGeneral;# how to store and solve the system of equations in the analysis
test NormDispIncr 1.0e-08 1000; # determine if convergence has been achieved at the end of an iteration step
#algorithm NewtonLineSearch;# use Newton's solution algorithm: updates tangent stiffness at every iteration
algorithm Newton;
set Dincr -0.01; #-0.00002
#integrator LoadControl 0.0001
#integrator ArcLength 0.05 1.0; #arclength alpha
                               #Node,  dof, 1st incr, Jd, min,   max
integrator DisplacementControl $EndNode 4   $Dincr     1  $Dincr -0.01;
analysis Static	;# define type of analysis static or transient
variable algorithmTypeStatic Newton
set ok [analyze 180];
puts "Finished"
#--------------------------------------------------------------------------------
#set finishTime [clock clicks -milliseconds];
#puts "Time taken: [expr ($finishTime-$startTime)/1000] sec"
#set systemTime [clock seconds] 
#puts "Finished Analysis: [clock format $systemTime -format "%d-%b-%Y %H:%M:%S"]"
