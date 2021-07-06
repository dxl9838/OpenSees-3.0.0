/* ****************************************************************** **
**    OpenSees - Open System for Earthquake Engineering Simulation    **
**          Pacific Earthquake Engineering Research Center            **
**                                                                    **
**                                                                    **
** (C) Copyright 1999, The Regents of the University of California    **
** All Rights Reserved.                                               **
**                                                                    **
** Commercial use of this program without express permission of the   **
** University of California, Berkeley, is strictly prohibited.  See   **
** file 'COPYRIGHT'  in main directory for information on usage and   **
** redistribution,  and for a DISCLAIMER OF ALL WARRANTIES.           **
**                                                                    **
** Developed by:                                                      **
**   Frank McKenna (fmckenna@ce.berkeley.edu)                         **
**   Gregory L. Fenves (fenves@ce.berkeley.edu)                       **
**   Filip C. Filippou (filippou@ce.berkeley.edu)                     **
**                                                                    **
** ****************************************************************** */

// $Revision: 1.1 $
// $Date: 2010-05-04 17:14:45 $
// $Source: /scratch/slocal/chroot/cvsroot/openseescomp/CompositePackages/mixedBeamColumn3d/mixedBeamColumn3d.cpp,v $

// Modified by: Xinlong Du, Northeastern University, USA; Year 2020
// Description: Adapted for analysis of asymmetric sections with introducing
// high-order axial terms for the basic element formulation

#include "mixedBeamColumn3d.h"
#include <elementAPI.h>
#include <G3Globals.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <iomanip>

#include <Information.h>
#include <MatrixUtil.h>
#include <Domain.h>
#include <Channel.h>
#include <FEM_ObjectBroker.h>
#include <Renderer.h>
#include <math.h>
#include <ElementResponse.h>
#include <ElementalLoad.h>
#include <iostream>
#include <fstream>
#include <Node.h>
#include <Message.h>

#include <LobattoBeamIntegration.h>
#include <LegendreBeamIntegration.h>
#include <RadauBeamIntegration.h>
#include <NewtonCotesBeamIntegration.h>
#include <TrapezoidalBeamIntegration.h>
#include <RegularizedHingeIntegration.h>

// Constants that define the dimensionality
//#define  NDM   3                       // dimension of the problem (3d)
//#define  NND   6                       // number of nodal dof's
//#define  NEGD  12                      // number of element global dof's
//#define  NDM_SECTION  3                // number of section dof's without torsion
//#define  NDM_NATURAL  5                // number of element dof's in the basic system without torsion
//#define  NDM_NATURAL_WITH_TORSION  6   // number of element dof's in the basic system with torsion
//#define  MAX_NUM_SECTIONS  10          // maximum number of sections allowed

using namespace std;


Matrix mixedBeamColumn3d::theMatrix(NEGD,NEGD);
Vector mixedBeamColumn3d::theVector(NEGD);
double mixedBeamColumn3d::workArea[400];
//Matrix mixedBeamColumn3d::transformNaturalCoords(NDM_NATURAL_WITH_TORSION,NDM_NATURAL_WITH_TORSION);
//Matrix mixedBeamColumn3d::transformNaturalCoordsT(NDM_NATURAL_WITH_TORSION,NDM_NATURAL_WITH_TORSION);
//int mixedBeamColumn3d::maxNumSections = 10;

Vector *mixedBeamColumn3d::sectionDefShapeFcn = 0;
Matrix *mixedBeamColumn3d::nldhat = 0;
Matrix *mixedBeamColumn3d::nd1 = 0;
Matrix *mixedBeamColumn3d::nd2 = 0;
Matrix *mixedBeamColumn3d::nd1T = 0;
Matrix *mixedBeamColumn3d::nd2T = 0;


/*
#ifdef _USRDLL
#include <windows.h>
#define OPS_Export extern "C" _declspec(dllexport)
#elif _MACOSX
#define OPS_Export extern "C" __attribute__((visibility("default")))
#else
#define OPS_Export extern "C"
#endif


OPS_Export void localInit() {
  OPS_Error("mixedBeamColumn3d element \nWritten by Mark D. Denavit, University of Illinois at Urbana-Champaign\n", 1);
}
*/

// Documentation: Three Dimensional Mixed Beam Column Element
// element mixedBeamColumn3d $tag $iNode $jNode $numIntgrPts $secTag $transfTag <-mass $massDens>
//   <-integration $intType> <-doRayleigh $rFlag> <-geomLinear>
//
// Required Input Parameters:
//   $tag                   integer tag identifying the element
//   $iNode, $jNode         end nodes
//   $numIntgrPts           number of integration points along the element length
//   $secTag                identifier for previously-defined section object
//   $transfTag             identifier for previously-defined coordinate-transformation (CrdTransf) object
//
// Optional Input:
//   -mass $massDens
//       $massDens          element mass density (per unit length), from which a lumped-mass matrix is formed (optional, default=0.0)
//   -integration $intType
//       $intType           numerical integration type, options are Lobotto, Legendre, Radau, NewtonCotes, Trapezoidal (optional, default= Lobotto)
//   -doRayleigh $rFlag
//       $rFlag             optional, default = 1
//                              rFlag = 0 no rayleigh damping
//                              rFlag = 1 include rayleigh damping (default)
//   -geomLinear            perform analysis without internal geometric nonlinearity
//   -shearCenter $ys $zs
//       $ys $zs            coordinates of shear center w.r.t centroid
//
//
// References:
//   1. Bulent N. Alemdar and Donald W. White, "Displacement, Flexibility, and Mixed Beam-Column Finite
//      Element Formulations for Distributed Plasticity Analysis," Journal of Structural Engineering 131,
//      no. 12 (December 2005): 1811-1819.
//   2. Cenk Tort and Jerome F. Hajjar, "Mixed Finite Element for Three-Dimensional Nonlinear Dynamic
//      Analysis of Rectangular Concrete-Filled Steel Tube Beam-Columns," Journal of Engineering Mechanics
//      136, no. 11 (November 0, 2010): 1329-1339.
//   3. Denavit, M. D. and Hajjar, J. F. (2010). "Nonlinear Seismic Analysis of Circular Concrete-Filled
//      Steel Tube Members and Frames," Report No. NSEL-023, Newmark Structural Laboratory Report Series
//      (ISSN 1940-9826), Department of Civil and Environmental Engineering, University of Illinois at
//      Urbana-Champaign, Urbana, Illinois, March.
//

void * OPS_mixedBeamColumn3d() {
  // Variables to retrieve input
  int iData[10];
  double dData[10];
  double dData2[2]; //input of ys and zs
  int sDataLength = 40;
  char *sData  = new char[sDataLength];
  char *sData2 = new char[sDataLength];
  int numData;

  // Check the number of dimensions
  if (OPS_GetNDM() != 3) {
     opserr << "ERROR: mixedBeamColumn3d: invalid number of dimensions\n";
   return 0;
  }

  // Check the number of degrees of freedom
  if (OPS_GetNDF() != 6) {
     opserr << "ERROR: mixedBeamColumn3d: invalid number of degrees of freedom\n";
   return 0;
  }

  // Check for minimum number of arguments
  if (OPS_GetNumRemainingInputArgs() < 6) {
    opserr << "ERROR: mixedBeamColumn3d: too few arguments\n";
    return 0;
  }

  // Get required input data
  numData = 6;
  if (OPS_GetIntInput(&numData, iData) != 0) {
    opserr << "WARNING invalid element data - mixedBeamColumn3d\n";
    return 0;
  }
  int eleTag = iData[0];
  int nodeI = iData[1];
  int nodeJ = iData[2];
  int numIntgrPts = iData[3];
  int secTag = iData[4];
  int transfTag = iData[5];

  // Get the section
  SectionForceDeformation *theSection = OPS_GetSectionForceDeformation(secTag);
  if (theSection == 0) {
    opserr << "WARNING section with tag " << secTag << "not found for element " << eleTag << endln;
    return 0;
  }

  SectionForceDeformation **sections = new SectionForceDeformation *[numIntgrPts];
  for (int i = 0; i < numIntgrPts; i++) {
    sections[i] = theSection;
  }

  // Get the coordinate transformation
  CrdTransf *theTransf = OPS_GetCrdTransf(transfTag);
  if (theTransf == 0) {
    opserr << "WARNING geometric transformation with tag " << transfTag << "not found for element " << eleTag << endln;
    return 0;
  }

  // Set Default Values for Optional Input
  int doRayleigh = 1;
  double massDens = 0.0;
  bool geomLinear = false;
  BeamIntegration *beamIntegr = 0;

  // Loop through remaining arguments to get optional input
  while ( OPS_GetNumRemainingInputArgs() > 0 ) {
    if ( OPS_GetStringCopy(&sData) != 0 ) {
      opserr << "WARNING invalid input";
      return 0;
    }

    if ( strcmp(sData,"-mass") == 0 ) {
      numData = 1;
      if (OPS_GetDoubleInput(&numData, dData) != 0) {
        opserr << "WARNING invalid input, want: -mass $massDens \n";
        return 0;
      }
      massDens = dData[0];

    } else if ( strcmp(sData,"-integration") == 0 ) {
      if ( OPS_GetStringCopy(&sData2) != 0 ) {
        opserr << "WARNING invalid input, want: -integration $intType";
        return 0;
      }

      if (strcmp(sData2,"Lobatto") == 0) {
        beamIntegr = new LobattoBeamIntegration();
      } else if (strcmp(sData2,"Legendre") == 0) {
        beamIntegr = new LegendreBeamIntegration();
      } else if (strcmp(sData2,"Radau") == 0) {
        beamIntegr = new RadauBeamIntegration();
      } else if (strcmp(sData2,"NewtonCotes") == 0) {
        beamIntegr = new NewtonCotesBeamIntegration();
      } else if (strcmp(sData2,"Trapezoidal") == 0) {
        beamIntegr = new TrapezoidalBeamIntegration();
      } else if (strcmp(sData2,"RegularizedLobatto") == 0 || strcmp(sData2,"RegLobatto") == 0) {
        numData = 4;
        if (OPS_GetDoubleInput(&numData, dData) != 0) {
          opserr << "WARNING invalid input, want: -integration RegularizedLobatto $lpI $lpJ $zetaI $zetaJ \n";
          return 0;
        }
        BeamIntegration *otherBeamInt = 0;
        otherBeamInt = new LobattoBeamIntegration();
        beamIntegr = new RegularizedHingeIntegration(*otherBeamInt, dData[0], dData[1], dData[2], dData[3]);
          if (otherBeamInt != 0) {
            delete otherBeamInt;
          }
      } else {
        opserr << "WARNING invalid integration type, element: " << eleTag;
        return 0;
      }

    } else if ( strcmp(sData,"-doRayleigh") == 0 ) {
        numData = 1;
        if (OPS_GetInt(&numData, &doRayleigh) != 0) {
          opserr << "WARNING: Invalid doRayleigh in element mixedBeamColumn3d " << eleTag;
          return 0;
        }

    } else if ( strcmp(sData,"-geomLinear") == 0 ) {
      geomLinear = true;

	} else if (strcmp(sData, "-shearCenter") == 0) {
		// Get the coordinates of shear center w.r.t centroid
		numData = 2;
		if (OPS_GetDoubleInput(&numData, &dData2[0]) < 0) {
			opserr << "WARNING: invalid ys and zs\n";
			return 0;
		}
	} else {
      opserr << "WARNING unknown option " << sData << "\n";
    }
  }


  // Set the beam integration object if not in options
  if (beamIntegr == 0) {
    beamIntegr = new LobattoBeamIntegration();
  }

  // now create the element and add it to the Domain
  Element *theElement = new mixedBeamColumn3d(eleTag, nodeI, nodeJ, numIntgrPts, sections, *beamIntegr, *theTransf,
	  dData2[0], dData2[1], massDens, doRayleigh, geomLinear);

  if (theElement == 0) {
    opserr << "WARNING ran out of memory creating element with tag " << eleTag << endln;
    return 0;
  }

  delete[] sections;
  if (beamIntegr != 0)
	  delete beamIntegr;
  delete[] sData; //This is temporary. See how to deal with charactor inputs in ForceBeamColume3d.cpp
  delete[] sData2;//or we can delete these charactor arrays after we understand these options of inputs for beam integration

  return theElement;
}


// constructor which takes the unique element tag, sections,
// and the node ID's of it's nodal end points.
// allocates the necessary space needed by each object
mixedBeamColumn3d::mixedBeamColumn3d (int tag, int nodeI, int nodeJ, int numSec,
                                      SectionForceDeformation **sec,
                                      BeamIntegration &bi,
                                      CrdTransf &coordTransf, double yss, double zss,
                                      double massDensPerUnitLength,
                                      int damp, bool geomLin):
  Element(tag,ELE_TAG_mixedBeamColumn3d),
  connectedExternalNodes(2), beamIntegr(0), numSections(0), sections(0),
  crdTransf(0), doRayleigh(damp), geomLinear(geomLin),
  rho(massDensPerUnitLength), initialLength(0.0),
  itr(0), initialFlag(0),
  V(NGF), committedV(NGF),
  internalForce(NEBD), committedInternalForce(NEBD),
  naturalForce(NGF), commitedNaturalForce(NGF),
  lastNaturalDisp(NEBD), commitedLastNaturalDisp(NEBD),
  sp(0),
  Hinv(NGF,NGF), commitedHinv(NGF,NGF),
  GMH(NGF,NEBD), commitedGMH(NGF,NEBD),
  kv(NEBD,NEBD), kvcommit(NEBD,NEBD),
  Ki(0),
  sectionForceFibers(0), commitedSectionForceFibers(0),
  sectionDefFibers(0), commitedSectionDefFibers(0),
  sectionFlexibility(0), commitedSectionFlexibility(0), sectionForceShapeFcn(0), ys(yss), zs(zss)
{
  theNodes[0] = 0;
  theNodes[1] = 0;

  connectedExternalNodes(0) = nodeI;
  connectedExternalNodes(1) = nodeJ;

  // get copy of the beam integration object
  beamIntegr = bi.getCopy();
  if (beamIntegr == 0) {
    opserr<<"Error: mixedBeamColumn3d::mixedBeamColumn3d: could not create copy of beam integration object" << endln;
    exit(-1);
  }

  // get copy of the transformation object
  crdTransf = coordTransf.getCopy3d();
  if (crdTransf == 0) {
    opserr << "Error: mixedBeamColumn3d::mixedBeamColumn3d: could not create copy of coordinate transformation object" << endln;
    exit(-1);
  }


  //this->setSectionPointers(numSec,sec);
  if (numSec > maxNumSections) {
    opserr << "Error: mixedBeamColumn3d::setSectionPointers -- max number of sections exceeded";
  }

  numSections = numSec;

  if (sec == 0) {
    opserr << "Error: mixedBeamColumn3d::setSectionPointers -- invalid section pointer";
  }

  sections = new SectionForceDeformation *[numSections];
  if (sections == 0) {
    opserr << "Error: mixedBeamColumn3d::setSectionPointers -- could not allocate section pointers";
  }

  for (int i = 0; i < numSections; i++) {
    if (sec[i] == 0) {
      opserr << "Error: mixedBeamColumn3d::setSectionPointers -- null section pointer " << i << endln;
    }

    sections[i] = (SectionForceDeformation*) sec[i]->getCopy();

    if (sections[i] == 0) {
      opserr << "Error: mixedBeamColumn3d::setSectionPointers -- could not create copy of section " << i << endln;
    }
  }

  p0[0] = 0.0;
  p0[1] = 0.0;
  p0[2] = 0.0;
  p0[3] = 0.0;
  p0[4] = 0.0;

  // Element vectors and matrices
  sectionForceFibers = new Vector [numSections];
  commitedSectionForceFibers = new Vector [numSections];
  sectionDefFibers = new Vector [numSections];
  commitedSectionDefFibers = new Vector [numSections];
  sectionFlexibility = new Matrix [numSections];
  commitedSectionFlexibility = new Matrix [numSections];

  for (int i = 0; i < numSections; i++){
    sectionForceFibers[i] = Vector(NSD);
    sectionForceFibers[i].Zero();
    commitedSectionForceFibers[i] = Vector(NSD);
    commitedSectionForceFibers[i].Zero();
    sectionDefFibers[i] = Vector(NSD);
    sectionDefFibers[i].Zero();
    commitedSectionDefFibers[i] = Vector(NSD);
    commitedSectionDefFibers[i].Zero();
    sectionFlexibility[i] = Matrix(NSD,NSD);
    sectionFlexibility[i].Zero();
    commitedSectionFlexibility[i] = Matrix(NSD,NSD);
    commitedSectionFlexibility[i].Zero();
  }

  V.Zero();
  internalForce.Zero();
  naturalForce.Zero();
  lastNaturalDisp.Zero();
  Hinv.Zero();
  GMH.Zero();
  kv.Zero();

  committedV.Zero();
  committedInternalForce.Zero();
  commitedNaturalForce.Zero();
  commitedLastNaturalDisp.Zero();
  commitedHinv.Zero();
  commitedGMH.Zero();
  kvcommit.Zero();

  if (sectionDefShapeFcn == 0)
    sectionDefShapeFcn  = new Vector [maxNumSections];
  if (nldhat == 0)
    nldhat  = new Matrix [maxNumSections];
  if (nd1 == 0)
    nd1  = new Matrix [maxNumSections];
  if (nd2 == 0)
    nd2  = new Matrix [maxNumSections];
  if (nd1T == 0)
    nd1T  = new Matrix [maxNumSections];
  if (nd2T == 0)
    nd2T  = new Matrix [maxNumSections];
  if (!sectionDefShapeFcn || !nldhat || !nd1 || !nd2 || !nd1T || !nd2T ) {
    opserr << "mixedBeamColumn3d::mixedBeamColumn3d() -- failed to allocate static section arrays";
    exit(-1);
  }

  int i;
  for ( i=0; i<maxNumSections; i++ ){
    nd1T[i] = Matrix(NGF,NSD);
    nd2T[i] = Matrix(NEBD,NSD);
  }

}

// constructor:
// invoked by a FEM_ObjectBroker, recvSelf() needs to be invoked on this object.
// CONSTRUCTOR FOR PARALLEL PROCESSING
mixedBeamColumn3d::mixedBeamColumn3d():
  Element(0,ELE_TAG_mixedBeamColumn3d),
  connectedExternalNodes(2), beamIntegr(0), numSections(0), sections(0), crdTransf(0), doRayleigh(0), geomLinear(false),
  rho(0.0), initialLength(0.0),
  itr(0), initialFlag(0),
  V(NGF), committedV(NGF),
  internalForce(NEBD), committedInternalForce(NEBD),
  naturalForce(NGF), commitedNaturalForce(NGF),
  lastNaturalDisp(NEBD), commitedLastNaturalDisp(NEBD),
  sp(0),
  Hinv(NGF,NGF), commitedHinv(NGF,NGF),
  GMH(NGF,NEBD), commitedGMH(NGF,NEBD),
  kv(NEBD,NEBD), kvcommit(NEBD,NEBD),
  Ki(0),
  sectionForceFibers(0), commitedSectionForceFibers(0), sectionDefFibers(0), commitedSectionDefFibers(0),
  sectionFlexibility(0), commitedSectionFlexibility(0), sectionForceShapeFcn(0), ys(0.0), zs(0.0)
{
  theNodes[0] = 0;
  theNodes[1] = 0;

  p0[0] = 0.0;
  p0[1] = 0.0;
  p0[2] = 0.0;
  p0[3] = 0.0;
  p0[4] = 0.0;

  // Element vectors and matrices
  sectionForceFibers = new Vector [numSections];
  commitedSectionForceFibers = new Vector [numSections];
  sectionDefFibers = new Vector [numSections];
  commitedSectionDefFibers = new Vector [numSections];
  sectionFlexibility = new Matrix [numSections];
  commitedSectionFlexibility = new Matrix [numSections];

  for (int i = 0; i < numSections; i++){
    sectionForceFibers[i] = Vector(NSD);
    sectionForceFibers[i].Zero();
    commitedSectionForceFibers[i] = Vector(NSD);
    commitedSectionForceFibers[i].Zero();
    sectionDefFibers[i] = Vector(NSD);
    sectionDefFibers[i].Zero();
    commitedSectionDefFibers[i] = Vector(NSD);
    commitedSectionDefFibers[i].Zero();
    sectionFlexibility[i] = Matrix(NSD,NSD);
    sectionFlexibility[i].Zero();
    commitedSectionFlexibility[i] = Matrix(NSD,NSD);
    commitedSectionFlexibility[i].Zero();
  }

  V.Zero();
  internalForce.Zero();
  naturalForce.Zero();
  lastNaturalDisp.Zero();
  Hinv.Zero();
  GMH.Zero();
  kv.Zero();

  committedV.Zero();
  committedInternalForce.Zero();
  commitedNaturalForce.Zero();
  commitedLastNaturalDisp.Zero();
  commitedHinv.Zero();
  commitedGMH.Zero();
  kvcommit.Zero();

  if (sectionDefShapeFcn == 0)
    sectionDefShapeFcn  = new Vector [maxNumSections];
  if (nldhat == 0)
    nldhat  = new Matrix [maxNumSections];
  if (nd1 == 0)
    nd1  = new Matrix [maxNumSections];
  if (nd2 == 0)
    nd2  = new Matrix [maxNumSections];
  if (nd1T == 0)
    nd1T  = new Matrix [maxNumSections];
  if (nd2T == 0)
    nd2T  = new Matrix [maxNumSections];
  if (!sectionDefShapeFcn || !nldhat || !nd1 || !nd2 || !nd1T || !nd2T ) {
    opserr << "mixedBeamColumn3d::mixedBeamColumn3d() -- failed to allocate static section arrays";
    exit(-1);
  }

  int i;
  for ( i=0; i<maxNumSections; i++ ){
    nd1T[i] = Matrix(NGF,NSD);
    nd2T[i] = Matrix(NEBD,NSD);
  }

}

mixedBeamColumn3d::~mixedBeamColumn3d() {

  if (sections) {
    for (int i=0; i < numSections; i++) {
      if (sections[i]) {
        delete sections[i];
      }
    }
    delete [] sections;
  }

  if (crdTransf)
    delete crdTransf;

  if (beamIntegr != 0)
    delete beamIntegr;

  if (sp != 0)
    delete sp;

  if (Ki != 0)
    delete Ki;

  if (sectionForceFibers != 0)
    delete [] sectionForceFibers;

  if (commitedSectionForceFibers != 0)
    delete [] commitedSectionForceFibers;

  if (sectionDefFibers != 0)
    delete [] sectionDefFibers;

  if (commitedSectionDefFibers != 0)
    delete [] commitedSectionDefFibers;

  if (sectionFlexibility != 0)
    delete [] sectionFlexibility;

  if (commitedSectionFlexibility != 0)
    delete [] commitedSectionFlexibility;
  if (sectionForceShapeFcn != 0)
	  delete[] sectionForceShapeFcn;
}

int mixedBeamColumn3d::getNumExternalNodes(void) const {
   return 2;
}

const ID & mixedBeamColumn3d::getExternalNodes(void) {
   return connectedExternalNodes;
}

Node ** mixedBeamColumn3d::getNodePtrs(void) {
   return theNodes;
}

int mixedBeamColumn3d::getNumDOF(void) {
   return NEGD;
}

void mixedBeamColumn3d::setDomain(Domain *theDomain) {

  // check Domain is not null - invoked when object removed from a domain
  if (theDomain == 0) {
    theNodes[0] = 0;
    theNodes[1] = 0;

    opserr << "mixedBeamColumn3d::setDomain:  theDomain = 0 ";
    exit(0);
  }

  // get pointers to the nodes
  int Nd1 = connectedExternalNodes(0);
  int Nd2 = connectedExternalNodes(1);

  theNodes[0] = theDomain->getNode(Nd1);
  theNodes[1] = theDomain->getNode(Nd2);

  if (theNodes[0] == 0) {
    opserr << "mixedBeamColumn3d::setDomain: Nd1: ";
    opserr << Nd1 << "does not exist in model\n";
    exit(0);
  }

  if (theNodes[1] == 0) {
    opserr << "mixedBeamColumn3d::setDomain: Nd2: ";
    opserr << Nd2 << "does not exist in model\n";
    exit(0);
  }

  // call the DomainComponent class method
  this->DomainComponent::setDomain(theDomain);

  // ensure connected nodes have correct number of dof's
  int dofNode1 = theNodes[0]->getNumberDOF();
  int dofNode2 = theNodes[1]->getNumberDOF();

  if ((dofNode1 != NND) || (dofNode2 != NND)) {
    opserr << "mixedBeamColumn3d::setDomain(): Nd2 or Nd1 incorrect dof ";
    exit(0);
  }

  // initialize the transformation
  if (crdTransf->initialize(theNodes[0], theNodes[1])) {
    opserr << "mixedBeamColumn3d::setDomain(): Error initializing coordinate transformation";
    exit(0);
  }

  // Check element length
  if (crdTransf->getInitialLength() == 0.0) {
    opserr << "mixedBeamColumn3d::setDomain(): Zero element length:" << this->getTag();
    exit(0);
  }

}

int mixedBeamColumn3d::commitState() {
  int err = 0; // error flag
  int i = 0; // integer for loops

  // call element commitState to do any base class stuff
  if ((err = this->Element::commitState()) != 0) {
    opserr << "mixedBeamColumn3d::commitState () - failed in base class";
    return err;
  }

  // commit the sections
  do {
    err = sections[i++]->commitState();
  } while (err == 0 && i < numSections);

  if (err)
    return err;

  // commit the transformation between coord. systems
  if ((err = crdTransf->commitState()) != 0)
    return err;

  // commit the element variables state
  committedV = V;
  committedInternalForce = internalForce;
  commitedNaturalForce = naturalForce;
  commitedLastNaturalDisp = lastNaturalDisp;
  commitedHinv = Hinv;
  commitedGMH = GMH;
  kvcommit = kv;
  for( i = 0; i < numSections; i++){
    commitedSectionForceFibers[i] = sectionForceFibers[i];
    commitedSectionDefFibers[i] = sectionDefFibers[i];
    commitedSectionFlexibility[i] = sectionFlexibility[i];
  }

  // Reset iteration counter
  itr = 0;

  return err;
}


int mixedBeamColumn3d::revertToLastCommit() {
  int err;
  int i = 0;

  do {
    err = sections[i]->revertToLastCommit();
    i++;
  } while (err == 0 && i < numSections);

  if (err)
    return err;

  // revert the transformation to last commit
  if ((err = crdTransf->revertToLastCommit()) != 0)
    return err;

  // revert the element state to last commit
  V = committedV;
  internalForce = committedInternalForce;
  naturalForce = commitedNaturalForce;
  lastNaturalDisp = commitedLastNaturalDisp;
  Hinv = commitedHinv;
  GMH = commitedGMH;
  kv   = kvcommit;
  for( i = 0; i < numSections; i++){
    sectionForceFibers[i] = commitedSectionForceFibers[i];
    sectionDefFibers[i] = commitedSectionDefFibers[i];
    sectionFlexibility[i] = commitedSectionFlexibility[i];
  }

  // Reset iteration counter
  itr = 0;

  return err;
}


int mixedBeamColumn3d::revertToStart()
{
  int err;
  int i,j,k; // for loops
  i = 0;

  // revert the sections state to start
  do {
     err = sections[i++]->revertToStart();
  } while (err == 0 && i < numSections);

  if (err)
    return err;

  // revert the transformation to start
  if ((err = crdTransf->revertToStart()) != 0)
    return err;

  // revert the element state to start

  // Set initial length
  initialLength = crdTransf->getInitialLength();

  // Get the numerical integration weights
  double wt[maxNumSections]; // weights of sections or gauss points of integration points
  beamIntegr->getSectionWeights(numSections, initialLength, wt);

  // Vector of zeros to use at initial natural displacements
  Vector myZeros(NEBD);
  myZeros.Zero();

  // Set initial shape functions
  for ( i = 0; i < numSections; i++ ){
    nldhat[i] = this->getNld_hat(i, myZeros, initialLength, geomLinear); //Xinlong: This need to be modified to consider Nldhat1 and Nldhat2
    nd1[i] = this->getNd1(i, myZeros, initialLength, geomLinear);
    nd2[i] = this->getNd2(i, 0.0, initialLength);
	nd1T[i].addMatrixTranspose(0.0, nd1[i], 1.0);  //Xinlong: nd1T=nd1T*0.0+nd1'*1.0
	nd2T[i].addMatrixTranspose(0.0, nd2[i], 1.0);
  }

  // Set initial and committed section flexibility and GJ
  Matrix ks(NSD,NSD);
  //double GJ;
  for ( i = 0; i < numSections; i++ ){
    //getSectionTangent(i,2,ks,GJ);
	ks = sections[i]->getInitialTangent();
    invertMatrix(NSD,ks,sectionFlexibility[i]);
    commitedSectionFlexibility[i] = sectionFlexibility[i];
  }

  // Set initial and committed section forces and deformations
  for ( i = 0; i < numSections; i++ ){
    sectionForceFibers[i].Zero();
    commitedSectionForceFibers[i].Zero();
    sectionDefFibers[i].Zero();
    commitedSectionDefFibers[i].Zero();
  }

  // Compute the following matrices: G, G2, H, H12, H22, Md, Kg
  Matrix G(NGF,NEBD);
  Matrix G2(NEBD,NEBD);
  Matrix H(NGF,NGF);
  Matrix H12(NGF,NEBD);
  Matrix H22(NEBD,NEBD);
  Matrix Md(NGF,NEBD);
  Matrix Kg(NEBD,NEBD);

  G.Zero();
  G2.Zero();
  H.Zero();
  H12.Zero();
  H22.Zero();
  Md.Zero();
  Kg.Zero();
  for( i = 0; i < numSections; i++ ){
    G   = G   + initialLength * wt[i] * nd1T[i] * nldhat[i];
    G2  = G2  + initialLength * wt[i] * nd2T[i] * nldhat[i];
    H   = H   + initialLength * wt[i] * nd1T[i] * sectionFlexibility[i] * nd1[i];
    H12 = H12 + initialLength * wt[i] * nd1T[i] * sectionFlexibility[i] * nd2[i];
    H22 = H22 + initialLength * wt[i] * nd2T[i] * sectionFlexibility[i] * nd2[i];
    // Md is zero since deformations are zero
    Kg  = Kg  + initialLength * wt[i] * this->getKg(i, 0, initialLength); //Xinlong: This isn't necessary. If P=0.0, Kg is zero
  }

  // Compute the inverse of the H matrix
  invertMatrix(NGF, H, Hinv);
  commitedHinv = Hinv;

  // Compute the GMH matrix ( G + Md - H12 ) and its transpose
  GMH = G + Md - H12;
  //GMH = G; // Omit P-small delta
  commitedGMH = GMH;

  // Compute the transposes of the following matrices: G2, GMH
  Matrix G2T(NEBD,NEBD);
  Matrix GMHT(NEBD,NGF);
  G2T.addMatrixTranspose(0.0, G2, 1.0);  //Xinlong: G2T=G2T*0.0+G2'*1.0
  GMHT.addMatrixTranspose(0.0, GMH, 1.0);
  /*
  // Compute the stiffness matrix without the torsion term
  Matrix K_temp_noT(NDM_NATURAL,NDM_NATURAL);
  K_temp_noT = ( Kg + G2 + G2T - H22 ) + GMHT * Hinv * GMH;
  //K_temp_noT = ( Kg ) + GMHT * Hinv * GMH; // Omit P-small delta

  // Add in the torsional stiffness term
  kv.Zero();
  for( i = 0; i < NDM_NATURAL; i++ ) {
    for( j = 0; j < NDM_NATURAL; j++ ) {
      kv(i,j) = K_temp_noT(i,j);
    }
  }

  kv(5,5) =  GJ/initialLength; // Torsional Stiffness GJ/L
  */
  kv.Zero();
  kv = (Kg + G2 + G2T - H22) + GMHT * Hinv * GMH;
  kvcommit = kv;

  //Matrix kvOpenSees = transformNaturalCoordsT*kv*transformNaturalCoords;
  Ki = new Matrix(crdTransf->getInitialGlobalStiffMatrix(kv));

  // Vector V is zero at initial state
  V.Zero();
  committedV.Zero();

  // Internal force is zero at initial state
  internalForce.Zero();
  committedInternalForce.Zero();
  naturalForce.Zero();
  commitedNaturalForce.Zero();

  // Last natural displacement is zero at initial state
  lastNaturalDisp.Zero();
  commitedLastNaturalDisp.Zero();

  // Reset iteration counter
  itr = 0;

  // Set initialFlag to 1 so update doesn't call again
  initialFlag = 1;

  return err;
}

const Matrix & mixedBeamColumn3d::getInitialStiff(void) {
  // If things haven't be initialized, then do so
  if (initialFlag == 0) {
    this->revertToStart();
  }
  return *Ki;
}

const Matrix & mixedBeamColumn3d::getTangentStiff(void) {
  // If things haven't be initialized, then do so
  if (initialFlag == 0) {
    this->revertToStart();
  }
  crdTransf->update();  // Will remove once we clean up the corotational 3d transformation -- MHS
  //Matrix ktOpenSees = transformNaturalCoordsT*kv*transformNaturalCoords;
  return crdTransf->getGlobalStiffMatrix(kv,internalForce);
}

const Vector & mixedBeamColumn3d::getResistingForce(void) {
  crdTransf->update();  // Will remove once we clean up the corotational 3d transformation -- MHS
  Vector p0Vec(p0, 5);
  return crdTransf->getGlobalResistingForce(internalForce, p0Vec);
}

int mixedBeamColumn3d::update() {

  // If things haven't be initialized, then do so
  if (initialFlag == 0) {
    this->revertToStart();
  }

  int i,j,k; // integers for loops

  // Update iteration counter
  // says how many times update has been called since the last commit state
  itr++;

  // Update Coordinate Transformation
  crdTransf->update();

  // Current Length
  double currentLength;
  if (geomLinear) {
    currentLength = initialLength;
  } else {
    currentLength = initialLength; //Xinlong: need to be cleaned later on
  }

  // Compute the natural displacements
  Vector naturalDisp = crdTransf->getBasicTrialDisp();
  //naturalDispWithTorsion = transformNaturalCoords*naturalDispWithTorsion;
    // convert to the arrangement of natural deformations that the element likes
  /*
  Vector naturalDisp(NDM_NATURAL);
  for ( i = 0; i < NDM_NATURAL; i++ ) {
    naturalDisp(i) = naturalDispWithTorsion(i); //all but the torsional component
  }
  double twist = naturalDispWithTorsion(5);
  */
  Vector naturalIncrDeltaDisp(NEBD);
  naturalIncrDeltaDisp = naturalDisp - lastNaturalDisp;
  lastNaturalDisp = naturalDisp;

  // Get the numerical integration weights
  double wt[maxNumSections]; // weights of sections or gauss points of integration points
  beamIntegr->getSectionWeights(numSections, initialLength, wt);

  // Define Variables
  //double GJ;
  //double torsionalForce;
  //Vector sectionForceShapeFcn[numSections];
  sectionForceShapeFcn = new Vector[numSections];
  for ( i = 0; i < numSections; i++ ) {
    sectionForceShapeFcn[i] = Vector(NSD);
  }

  // Compute shape functions and their transposes
  for ( i = 0; i < numSections; i++ ){
    // Shape Functions
    nldhat[i] = this->getNld_hat(i, naturalDisp, currentLength, geomLinear);
    sectionDefShapeFcn[i] = this->getd_hat(i, naturalDisp, currentLength, geomLinear);
    nd1[i] = this->getNd1(i, naturalDisp, currentLength, geomLinear);
    if (geomLinear) {
      nd2[i].Zero();
    } else {
      nd2[i] = this->getNd2(i, internalForce(0), currentLength);
    }

    // Transpose of shape functions
	nd1T[i].addMatrixTranspose(0.0, nd1[i], 1.0);  //Xinlong: nd1T=nd1T*0.0+nd1'*1.0
	nd2T[i].addMatrixTranspose(0.0, nd2[i], 1.0);
  }

  // Update natural force
  if (geomLinear) {
    naturalForce = naturalForce + Hinv * ( GMH * naturalIncrDeltaDisp + V );
  } else {
    naturalForce = naturalForce + Hinv * ( GMH * naturalIncrDeltaDisp + V );
  }


  // Update sections
  for ( i = 0; i < numSections; i++){
    // Compute section deformations
    sectionForceShapeFcn[i] = nd1[i] * naturalForce;
    if (sp != 0) {
      const Matrix &s_p = *sp;
      for ( j = 0; j < NSD; j++ ) {
        sectionForceShapeFcn[i](j) += s_p(j,i);
      }
    }
    sectionDefFibers[i] = sectionDefFibers[i] + sectionFlexibility[i] * ( sectionForceShapeFcn[i] - sectionForceFibers[i] );

    // Send section deformation to section object
    //double torsionalStrain = twist/currentLength;
    //setSectionDeformation(i,sectionDefFibers[i],torsionalStrain);
	if (sections[i]->setTrialSectionDeformation(sectionDefFibers[i]) < 0) {
		opserr << "mixedBeamColumn3d::update() - section failed in setTrial\n";
		return -1;
	}

    // Get section force vector
    //double tempTorsionalForce;
    //getSectionStress(i,sectionForceFibers[i],tempTorsionalForce);
    //if (i == 0) {
    //  torsionalForce = tempTorsionalForce;
    //}
	sectionForceFibers[i] = sections[i]->getStressResultant();

    // Get section tangent matrix
    Matrix ks(NSD,NSD);
    //getSectionTangent(i,1,ks,GJ);
	ks = sections[i]->getSectionTangent();

    // Compute section flexibility matrix
    invertMatrix(NSD,ks,sectionFlexibility[i]);
  }

  // Compute the following matrices: V, V2, G, G2, H, H12, H22, Md, Kg
  Vector V2(NEBD);
  Matrix G(NGF,NEBD);
  Matrix G2(NEBD,NEBD);
  Matrix H(NGF,NGF);
  Matrix H12(NGF,NEBD);
  Matrix H22(NEBD,NEBD);
  Matrix Md(NGF,NEBD);
  Matrix Kg(NEBD,NEBD);

  V.Zero();
  V2.Zero();
  G.Zero();
  G2.Zero();
  H.Zero();
  H12.Zero();
  H22.Zero();
  Md.Zero();
  Kg.Zero();

  for( i = 0; i < numSections; i++ ){
    V   = V   + initialLength * wt[i] * nd1T[i] * (sectionDefShapeFcn[i] - sectionDefFibers[i] - sectionFlexibility[i] * ( sectionForceShapeFcn[i] - sectionForceFibers[i] ) );
    V2  = V2  + initialLength * wt[i] * nd2T[i] * (sectionDefShapeFcn[i] - sectionDefFibers[i]);
    G   = G   + initialLength * wt[i] * nd1T[i] * nldhat[i];
    G2  = G2  + initialLength * wt[i] * nd2T[i] * nldhat[i];
    H   = H   + initialLength * wt[i] * nd1T[i] * sectionFlexibility[i] * nd1[i];
    H12 = H12 + initialLength * wt[i] * nd1T[i] * sectionFlexibility[i] * nd2[i];
    H22 = H22 + initialLength * wt[i] * nd2T[i] * sectionFlexibility[i] * nd2[i];
    if (!geomLinear) {
      Kg = Kg  + initialLength * wt[i] * this->getKg(i, sectionForceFibers[i], currentLength);
        // sectionForceFibers[i](0) is the axial load, P
      Md = Md  + initialLength * wt[i] * this->getMd(i, sectionDefShapeFcn[i], sectionDefFibers[i], currentLength);
    }
  }

  // Compute the inverse of the H matrix
  invertMatrix(NGF, H, Hinv);

  // Compute the GMH matrix ( G + Md - H12 ) and its transpose
  GMH = G + Md - H12;
  //GMH = G; // Omit P-small delta

  // Compute the transposes of the following matrices: G, G2, GMH
  Matrix GT(NEBD,NGF);
  Matrix G2T(NEBD,NEBD);
  Matrix GMHT(NEBD,NGF);
  GT.addMatrixTranspose(0.0, G, 1.0);
  G2T.addMatrixTranspose(0.0, G2, 1.0);  //Xinlong: G2T=G2T*0.0+G2'*1.0
  GMHT.addMatrixTranspose(0.0, GMH, 1.0);

  // Compute new internal force
  //Vector internalForce(NEBD);
  //internalForce.Zero();

  if (geomLinear) {
    internalForce = GT * naturalForce + V2 + GMHT * Hinv * V;
  } else {
    internalForce = GT * naturalForce + V2 + GMHT * Hinv * V;
  }
  /*
  // Compute internal force for OpenSees ( i.e., add torsion and rearrange )
  for ( i = 0; i < NDM_NATURAL; i++ ) {
    internalForceOpenSees(i) = internalForce(i);
  }
  internalForceOpenSees(5) = torsionalForce; // Add in torsional force
  internalForceOpenSees = transformNaturalCoordsT*internalForceOpenSees;

  // Compute the stiffness matrix without the torsion term
  Matrix K_temp(NDM_NATURAL,NDM_NATURAL);
  if (geomLinear) {
    K_temp = ( Kg + G2 + G2T - H22 ) + GMHT * Hinv * GMH;
  } else {
    K_temp = ( Kg + G2 + G2T - H22 ) + GMHT * Hinv * GMH;
  }

  // Add in the torsional stiffness term
  kv.Zero();
  for( i = 0; i < NDM_NATURAL; i++ ) {
    for( j = 0; j< NDM_NATURAL; j++ ) {
      kv(i,j) = K_temp(i,j);
    }
  }

  kv(5,5) =  GJ/currentLength; // Torsional Stiffness GJ/L
  */
  kv.Zero();
  if (geomLinear) {
	  kv = (Kg + G2 + G2T - H22) + GMHT * Hinv * GMH;
  }
  else {
	  kv = (Kg + G2 + G2T - H22) + GMHT * Hinv * GMH;
  }

  return 0;
}

const Matrix & mixedBeamColumn3d::getMass(void) {
  theMatrix.Zero();

  if (rho != 0.0) {
    theMatrix(0,0) = theMatrix(1,1) = theMatrix(2,2) =
    theMatrix(6,6) = theMatrix(7,7) = theMatrix(8,8) = 0.5*initialLength*rho;
  }

  return theMatrix;
}

const Matrix & mixedBeamColumn3d::getDamp(void) {
  theMatrix.Zero();

  // Add the damping forces
  if ( doRayleigh == 1 && (alphaM != 0.0 || betaK != 0.0 || betaK0 != 0.0 || betaKc != 0.0) ) {
    theMatrix = this->Element::getDamp();
  }

  return theMatrix;
}

void mixedBeamColumn3d::zeroLoad(void) {
  if (sp != 0)
    sp->Zero();

  p0[0] = 0.0;
  p0[1] = 0.0;
  p0[2] = 0.0;
  p0[3] = 0.0;
  p0[4] = 0.0;
}

int mixedBeamColumn3d::addLoad(ElementalLoad *theLoad, double loadFactor) {

  int type;
  const Vector &data = theLoad->getData(type, loadFactor);

  if (sp == 0) {
    sp = new Matrix(NSD,numSections);
    if (sp == 0) {
      opserr << "mixedBeamColumn3d::addLoad -- out of memory\n";
      exit(-1);
    }
  }

  double L = crdTransf->getInitialLength();

  double xi[maxNumSections];
  beamIntegr->getSectionLocations(numSections, L, xi);

  if (type == LOAD_TAG_Beam3dUniformLoad) {
    double wy = data(0)*loadFactor;  // Transverse
    double wz = data(1)*loadFactor;  // Transverse
    double wx = data(2)*loadFactor;  // Axial

    Matrix &s_p = *sp;

    // Accumulate applied section forces due to element loads
    for (int i = 0; i < numSections; i++) {
      double x = xi[i]*L;
      // Axial
      s_p(0,i) += wx*(L-x);
      // Moment
      s_p(1,i) += wy*0.5*x*(x-L);
      // Moment
      s_p(2,i) += wz*0.5*x*(L-x);
    }

    // Accumulate reactions in basic system
    p0[0] -= wx*L;
    double V;
    V = 0.5*wy*L;
    p0[1] -= V;
    p0[2] -= V;
    V = 0.5*wz*L;
    p0[3] -= V;
    p0[4] -= V;


  } else if (type == LOAD_TAG_Beam3dPointLoad) {
    double Py = data(0)*loadFactor;
    double Pz = data(1)*loadFactor;
    double N  = data(2)*loadFactor;
    double aOverL = data(3);

    if (aOverL < 0.0 || aOverL > 1.0)
      return 0;

    double a = aOverL*L;

    double Vy2 = Py*aOverL;
    double Vy1 = Py-Vy2;

    double Vz2 = Pz*aOverL;
    double Vz1 = Pz-Vz2;

    Matrix &s_p = *sp;

    // Accumulate applied section forces due to element loads
    for (int i = 0; i < numSections; i++) {
      double x = xi[i]*L;
      if (x <= a) {
        s_p(0,i) += N;
        s_p(1,i) -= x*Vy1;
        s_p(2,i) += x*Vz1;
      }
      else {
        s_p(1,i) -= (L-x)*Vy2;
        s_p(2,i) += (L-x)*Vz2;
      }
    }

    // Accumulate reactions in basic system
    p0[0] -= N;
    p0[1] -= Vy1;
    p0[2] -= Vy2;
    p0[3] -= Vz1;
    p0[4] -= Vz2;


  } else {
    opserr << "mixedBeamColumn3d::addLoad() -- load type unknown for element with tag: " <<
        this->getTag() << endln;

    return -1;
  }

  return 0;
}

const Vector & mixedBeamColumn3d::getResistingForceIncInertia() {

  // Compute the current resisting force
  theVector = this->getResistingForce();

  // Add the inertial forces
  if (rho != 0.0) {
    const Vector &accel1 = theNodes[0]->getTrialAccel();
    const Vector &accel2 = theNodes[1]->getTrialAccel();

    double L = crdTransf->getInitialLength();
    double m = 0.5*rho*L;

    theVector(0) += m*accel1(0);
    theVector(1) += m*accel1(1);
    theVector(2) += m*accel1(2);
    theVector(6) += m*accel2(0);
    theVector(7) += m*accel2(1);
    theVector(8) += m*accel2(2);
  }

  // Add the damping forces
  if ( doRayleigh == 1 && (alphaM != 0.0 || betaK != 0.0 || betaK0 != 0.0 || betaKc != 0.0) ) {
    theVector += this->getRayleighDampingForces();
  }

  return theVector;
}


void mixedBeamColumn3d::Print(OPS_Stream &s, int flag) {

  if (flag == 1) {
    s << "\nElement: " << this->getTag() << " Type: mixedBeamColumn3d ";
    s << "\tConnected Nodes: " << connectedExternalNodes ;
    s << "\tNumber of Sections: " << numSections;
    s << "\tMass density: " << rho;
    for (int i = 0; i < numSections; i++)
      s << "\nSection "<<i<<" :" << *sections[i];

  } else if (flag == 33) {
    s << "\nElement: " << this->getTag() << " Type: mixedBeamColumn3d ";
    double xi[maxNumSections]; // location of sections or gauss points or integration points
    beamIntegr->getSectionLocations(numSections, initialLength, xi);
    double wt[maxNumSections]; // weights of sections or gauss points of integration points
    beamIntegr->getSectionWeights(numSections, initialLength, wt);
    s << "\n section xi wt";
    for (int i = 0; i < numSections; i++)
      s << "\n"<<i<<" "<<xi[i]<<" "<<wt[i];

  } else if (flag == OPS_PRINT_PRINTMODEL_JSON) {
    s << "\t\t\t{";
    s << "\"name\": " << this->getTag() << ", ";
    s << "\"type\": \"mixedBeamColumn2d\", ";
    s << "\"nodes\": [" << connectedExternalNodes(0) << ", " << connectedExternalNodes(1) << "], ";
    s << "\"sections\": [";
    for (int i = 0; i < numSections - 1; i++)
      s << "\"" << sections[i]->getTag() << "\", ";
    s << "\"" << sections[numSections - 1]->getTag() << "\"], ";
    s << "\"integration\": ";
    beamIntegr->Print(s, flag);
    s << ", \"massperlength\": " << rho << ", ";
    s << "\"crdTransformation\": \"" << crdTransf->getTag() << "\"";
    if (!doRayleigh)
      s << ", \"doRayleigh\": false";
    if (geomLinear)
      s << ", \"geomLinear\": true";
    s << "}";

  } else {
    s << "\nElement: " << this->getTag() << " Type: mixedBeamColumn3d ";
    s << "\tConnected Nodes: " << connectedExternalNodes ;
    s << "\tNumber of Sections: " << numSections;
    s << "\tMass density: " << rho << endln;
  }

}


OPS_Stream &operator<<(OPS_Stream &s, mixedBeamColumn3d &E) {
  E.Print(s);
  return s;
}


Response* mixedBeamColumn3d::setResponse(const char **argv, int argc,
                                         OPS_Stream &output) {

  Response *theResponse = 0;

  output.tag("ElementOutput");
  output.attr("eleType","mixedBeamColumn3d");
  output.attr("eleTag",this->getTag());
  output.attr("node1",connectedExternalNodes[0]);
  output.attr("node2",connectedExternalNodes[1]);

  //
  // we compare argv[0] for known response types
  //

  // global force -
  if (strcmp(argv[0],"forces") == 0 ||
      strcmp(argv[0],"force") == 0 ||
      strcmp(argv[0],"globalForce") == 0 ||
      strcmp(argv[0],"globalForces") == 0) {

    output.tag("ResponseType","Px_1");
    output.tag("ResponseType","Py_1");
    output.tag("ResponseType","Pz_1");
    output.tag("ResponseType","Mx_1");
    output.tag("ResponseType","My_1");
    output.tag("ResponseType","Mz_1");
    output.tag("ResponseType","Px_2");
    output.tag("ResponseType","Py_2");
    output.tag("ResponseType","Pz_2");
    output.tag("ResponseType","Mx_2");
    output.tag("ResponseType","My_2");
    output.tag("ResponseType","Mz_2");

    theResponse = new ElementResponse(this, 1, theVector);

  // local force -
  } else if (strcmp(argv[0],"localForce") == 0 ||
             strcmp(argv[0],"localForces") == 0) {

    output.tag("ResponseType","N_ 1");
    output.tag("ResponseType","Vy_1");
    output.tag("ResponseType","Vz_1");
    output.tag("ResponseType","T_1");
    output.tag("ResponseType","My_1");
    output.tag("ResponseType","Tz_1");
    output.tag("ResponseType","N_2");
    output.tag("ResponseType","Py_2");
    output.tag("ResponseType","Pz_2");
    output.tag("ResponseType","T_2");
    output.tag("ResponseType","My_2");
    output.tag("ResponseType","Mz_2");

    theResponse = new ElementResponse(this, 2, theVector);

  // basic or natural forces
  } else if (strcmp(argv[0],"basicForce") == 0 ||
             strcmp(argv[0],"basicForces") == 0) {

    output.tag("ResponseType","N");
    output.tag("ResponseType","Mz_1");
    output.tag("ResponseType","Mz_2");
    output.tag("ResponseType","My_1");
    output.tag("ResponseType","My_2");
    output.tag("ResponseType","T");

    theResponse = new ElementResponse(this, 3, Vector(6));
  } else if (strcmp(argv[0],"sectionDeformation_Force") == 0) {

    int i;
    char *q  = new char[15];
    for ( i = 0; i < numSections; i++ ){
      sprintf(q,"axialStrain_%i",i+1);
      output.tag("ResponseType",q);
      sprintf(q,"curvatureZ_%i",i+1);
      output.tag("ResponseType",q);
      sprintf(q,"curvatureY_%i",i+1);
      output.tag("ResponseType",q);
    }
    delete [] q;

    theResponse =  new ElementResponse(this, 4, Vector(3*numSections));

  } else if (strcmp(argv[0],"plasticSectionDeformation_Force") == 0) {

    int i;
    char *q  = new char[25];
    for ( i = 0; i < numSections; i++ ){
      sprintf(q,"plasticAxialStrain_%i",i+1);
      output.tag("ResponseType",q);
      sprintf(q,"plasticCurvatureZ_%i",i+1);
      output.tag("ResponseType",q);
      sprintf(q,"plasticCurvatureY_%i",i+1);
      output.tag("ResponseType",q);
    }
    delete [] q;

    theResponse =  new ElementResponse(this, 5, Vector(3*numSections));

  } else if (strcmp(argv[0],"integrationPoints") == 0) {
    theResponse =  new ElementResponse(this, 100, Vector(numSections));

  } else if (strcmp(argv[0],"integrationWeights") == 0) {
    theResponse =  new ElementResponse(this, 101, Vector(numSections));

  } else if (strcmp(argv[0],"connectedNodes") == 0) {
    theResponse =  new ElementResponse(this, 102, Vector(2));

  } else if (strcmp(argv[0],"numSections") == 0 ||
             strcmp(argv[0],"numberOfSections") == 0 ) {
    theResponse =  new ElementResponse(this, 103, Vector(1));

  } else if (strcmp(argv[0],"section") ==0) {
    if (argc > 2) {

      int sectionNum = atoi(argv[1]);
      if (sectionNum > 0 && sectionNum <= numSections) {

        double xi[maxNumSections];
        double L = crdTransf->getInitialLength();
        beamIntegr->getSectionLocations(numSections, L, xi);

        output.tag("GaussPointOutput");
        output.attr("number",sectionNum);
        output.attr("eta",xi[sectionNum-1]*L);

        theResponse =  sections[sectionNum-1]->setResponse(&argv[2], argc-2, output);

        output.endTag();
      }
    }
  }

  output.endTag();
  return theResponse;
}


int mixedBeamColumn3d::getResponse(int responseID, Information &eleInfo) {
  if (responseID == 1) { // global forces
    return eleInfo.setVector(this->getResistingForce());

  } else if (responseID == 2) { // local forces
    // Axial
    double N = internalForce(0);
    theVector(6) =  N;
    theVector(0) = -N+p0[0];

    // Torsion
    double T = internalForce(5);
    theVector(9) =  T;
    theVector(3) = -T;

    // Moments about z and shears along y
    double M1 = internalForce(1);
    double M2 = internalForce(2);
    theVector(5)  = M1;
    theVector(11) = M2;
    double L = crdTransf->getInitialLength();
    double V = (M1+M2)/L;
    theVector(1) =  V+p0[1];
    theVector(7) = -V+p0[2];

    // Moments about y and shears along z
    M1 = internalForce(3);
    M2 = internalForce(4);
    theVector(4)  = M1;
    theVector(10) = M2;
    V = -(M1+M2)/L;
    theVector(2) = -V+p0[3];
    theVector(8) =  V+p0[4];

    return eleInfo.setVector(theVector);

  } else if (responseID == 3) { // basic forces
    return eleInfo.setVector(internalForce);

  } else if (responseID == 4) { // section deformation (from forces)

    int i;
    Vector tempVector(3*numSections);
    tempVector.Zero();
    for ( i = 0; i < numSections; i++ ){
      tempVector(3*i)   = sectionDefFibers[i](0);
      tempVector(3*i+1) = sectionDefFibers[i](1);
      tempVector(3*i+2) = sectionDefFibers[i](2);
    }

    return eleInfo.setVector(tempVector);

  } else if (responseID == 5) { // plastic section deformation (from forces)

    int i;
    Vector tempVector(3*numSections);
    Vector sectionForce(NSD);
    Vector plasticSectionDef(NSD);
    Matrix ks(NSD,NSD);
    Matrix fs(NSD,NSD);
    tempVector.Zero();
    double scratch = 0.0;
    for ( i = 0; i < numSections; i++ ){
	  sectionForce = sections[i]->getStressResultant();
	  ks = sections[i]->getInitialTangent();
      invertMatrix(NSD,ks,fs);

      plasticSectionDef = sectionDefFibers[i] - fs*sectionForce;

      tempVector(3*i)   = plasticSectionDef(0);
      tempVector(3*i+1) = plasticSectionDef(1);
      tempVector(3*i+2) = plasticSectionDef(2);
    }

    return eleInfo.setVector(tempVector);

  } else if (responseID == 100) { // integration points

    double L = crdTransf->getInitialLength();
    double pts[maxNumSections];
    beamIntegr->getSectionLocations(numSections, L, pts);
    Vector locs(numSections);
    for (int i = 0; i < numSections; i++)
      locs(i) = pts[i]*L;
    return eleInfo.setVector(locs);

  } else if (responseID == 101) { // integration weights
      double L = crdTransf->getInitialLength();
      double wts[maxNumSections];
      beamIntegr->getSectionWeights(numSections, L, wts);
      Vector weights(numSections);
      for (int i = 0; i < numSections; i++)
        weights(i) = wts[i]*L;
      return eleInfo.setVector(weights);

  } else if (responseID == 102) { // connected nodes
    Vector tempVector(2);
    tempVector(0) = connectedExternalNodes(0);
    tempVector(1) = connectedExternalNodes(1);
    return eleInfo.setVector(tempVector);

  } else if (responseID == 103) { // number of sections
    Vector tempVector(1);
    tempVector(0) = numSections;
    return eleInfo.setVector(tempVector);

  } else {
    return -1;

  }
}

Vector mixedBeamColumn3d::getd_hat(int sec, const Vector &v, double L, bool geomLinear) {
  double xi[maxNumSections];
  beamIntegr->getSectionLocations(numSections, L, xi);

  Vector D_hat(NSD);
  D_hat.Zero();

  double oneOverL = 1.0 / L;
  double xi1 = xi[sec];
  double dNv1 = 1.0 + 3.0*xi1*xi1 - 4.0*xi1;
  double ddNv1 = 6.0*xi1*oneOverL - 4.0*oneOverL;
  double dNv2 = 3.0*xi1*xi1 - 2.0*xi1;
  double ddNv2 = 6.0*xi1*oneOverL - 2.0*oneOverL;
  double dNw1 = -dNv1;
  double ddNw1 = -ddNv1;
  double dNw2 = -dNv2;
  double ddNw2 = -ddNv2;
  double Nf1 = xi1;

  double e0 = oneOverL * v(0); //u'
  double e1 = ddNv1 * v(1) + ddNv2 * v(2); //v"
  double e2 = ddNw1 * v(3) + ddNw2 * v(4); //w"
  double e3 = oneOverL * v(5); //phi'
  double e4 = dNv1 * v(1) + dNv2 * v(2); //v'
  double e5 = dNw1 * v(3) + dNw2 * v(4); //w'
  double e6 = Nf1 * v(5); //phi

  if (geomLinear) {
	  D_hat(0) = e0;
	  D_hat(1) = e1;
	  D_hat(2) = -e2;
  } else {
	  D_hat(0) = e0 + 0.5*(e4*e4 + e5 * e5) + (zs*e4 - ys * e5)*e3;
	  D_hat(1) = e1 + e2 * e6;
	  D_hat(2) = -e2 + e1 * e6;
	  D_hat(3) = 0.5*e3*e3;
	  D_hat(4) = e3;
  }

  return D_hat;
}

Matrix mixedBeamColumn3d::getKg(int sec, Vector P, double L) {
  double xi[maxNumSections];
  beamIntegr->getSectionLocations(numSections, L, xi);

  Matrix kg(NEBD,NEBD);
  Matrix N2(NGF, NEBD);
  Matrix Gmax(NGF, NGF);
  kg.Zero();
  N2.Zero();
  Gmax.Zero();

  double oneOverL = 1.0 / L;
  double xi1 = xi[sec];
  double dNv1 = 1.0 + 3.0*xi1*xi1 - 4.0*xi1;
  double ddNv1 = 6.0*xi1*oneOverL - 4.0*oneOverL;
  double dNv2 = 3.0*xi1*xi1 - 2.0*xi1;
  double ddNv2 = 6.0*xi1*oneOverL - 2.0*oneOverL;
  double dNw1 = -dNv1;
  double ddNw1 = -ddNv1;
  double dNw2 = -dNv2;
  double ddNw2 = -ddNv2;
  double Nf1 = xi1;

  //Matrix Ndeltad2-------------------------------------------------------
  N2(0, 0) = oneOverL;
  N2(1, 1) = dNv1;
  N2(1, 2) = dNv2;
  N2(2, 3) = dNw1;
  N2(2, 4) = dNw2;
  N2(3, 1) = ddNv1;
  N2(3, 2) = ddNv2;
  N2(4, 3) = ddNw1;
  N2(4, 4) = ddNw2;
  N2(5, 5) = Nf1;
  N2(6, 5) = oneOverL;

  Gmax(1, 1) = Gmax(2, 2) = P(0); //N
  Gmax(5, 4) = Gmax(4, 5) = P(1); //Mz
  Gmax(5, 3) = Gmax(3, 5) = P(2); //My
  Gmax(6, 1) = Gmax(1, 6) = P(0)*zs; //Nzs
  Gmax(6, 2) = Gmax(2, 6) = -P(0)*ys; //-Nys
  Gmax(6, 6) = P(3); //W

  kg.addMatrixTripleProduct(0.0, N2, Gmax, 1.0);

  return kg;
}

Matrix mixedBeamColumn3d::getMd(int sec, Vector dShapeFcn, Vector dFibers, double L) {
  double xi[maxNumSections];
  beamIntegr->getSectionLocations(numSections, L, xi);

  Matrix md(NGF,NEBD);
  md.Zero();

  double x = L * xi[sec];
  double Nv1 = x * (1 - x / L)*(1 - x / L);
  double Nv2 = x * x / L * (x / L - 1);
  double Nw1 = -Nv1;
  double Nw2 = -Nv2;

  md(0,1) = Nv1 * ( dShapeFcn(1) - dFibers(1) );
  md(0,2) = Nv2 * ( dShapeFcn(1) - dFibers(1) );
  md(0,3) = -Nw1 * ( dShapeFcn(2) - dFibers(2) );
  md(0,4) = -Nw2 * ( dShapeFcn(2) - dFibers(2) );

  return md;
}

Matrix mixedBeamColumn3d::getNld_hat(int sec, const Vector &v, double L, bool geomLinear) {
  double xi[maxNumSections];
  beamIntegr->getSectionLocations(numSections, L, xi);

  Matrix Nld_hat(NSD,NEBD);
  Matrix N1(NSD, NGF);
  Matrix N2(NGF, NEBD);
  Nld_hat.Zero();
  N1.Zero();
  N2.Zero();

  double oneOverL = 1.0 / L;
  double xi1 = xi[sec];
  double dNv1 = 1.0 + 3.0*xi1*xi1 - 4.0*xi1;
  double ddNv1 = 6.0*xi1*oneOverL - 4.0*oneOverL;
  double dNv2 = 3.0*xi1*xi1 - 2.0*xi1;
  double ddNv2 = 6.0*xi1*oneOverL - 2.0*oneOverL;
  double dNw1 = -dNv1;
  double ddNw1 = -ddNv1;
  double dNw2 = -dNv2;
  double ddNw2 = -ddNv2;
  double Nf1 = xi1;

  double dv = dNv1 * v(1) + dNv2 * v(2); //v'
  double ddv = ddNv1 * v(1) + ddNv2 * v(2); //v"
  double dw = dNw1 * v(3) + dNw2 * v(4); //w'
  double ddw = ddNw1 * v(3) + ddNw2 * v(4); //w"
  double f = Nf1 * v(5); //phi
  double df = oneOverL * v(5); //phi'

  if (geomLinear) {
	  //Matrix Ndeltad1-------------------------------------------------------
	  N1(0, 0) = 1.0;
	  N1(1, 3) = 1.0;
	  N1(2, 4) = -1.0;
  } else {
	  //Matrix Ndeltad1-------------------------------------------------------
	  N1(0, 0) = 1.0;
	  N1(0, 1) = dv + zs * df;
	  N1(0, 2) = dw - ys * df;
	  N1(0, 6) = zs * dv - ys * dw;
	  N1(1, 3) = 1.0;
	  N1(1, 4) = f;
	  N1(1, 5) = ddw;
	  N1(2, 3) = f;
	  N1(2, 4) = -1.0;
	  N1(2, 5) = ddv;
	  N1(3, 6) = df;
	  N1(4, 6) = 1.0;
  }
  //Matrix Ndeltad2-------------------------------------------------------
  N2(0, 0) = oneOverL;
  N2(1, 1) = dNv1;
  N2(1, 2) = dNv2;
  N2(2, 3) = dNw1;
  N2(2, 4) = dNw2;
  N2(3, 1) = ddNv1;
  N2(3, 2) = ddNv2;
  N2(4, 3) = ddNw1;
  N2(4, 4) = ddNw2;
  N2(5, 5) = Nf1;
  N2(6, 5) = oneOverL;

  Nld_hat.addMatrixProduct(0.0, N1, N2, 1.0); //N1*N2

  return Nld_hat;
}

Matrix mixedBeamColumn3d::getNd2(int sec, double P, double L) {
  double xi[maxNumSections];
  beamIntegr->getSectionLocations(numSections, L, xi);

  Matrix Nd2(NSD,NEBD);
  Nd2.Zero();

  double x = L * xi[sec];
  double Nv1 = x * (1 - x / L)*(1 - x / L);
  double Nv2 = x * x / L * (x / L - 1);
  double Nw1 = -Nv1;
  double Nw2 = -Nv2;

  Nd2(1, 1) = P * Nv1;
  Nd2(1, 2) = P * Nv2;
  Nd2(2, 3) = -P * Nw1;
  Nd2(2, 4) = -P * Nw2;

  return Nd2;
}

Matrix mixedBeamColumn3d::getNd1(int sec, const Vector &v, double L, bool geomLinear) {
  double xi[maxNumSections];
  beamIntegr->getSectionLocations(numSections, L, xi);

  Matrix Nd1(NSD,NGF);
  Nd1.Zero();

  double x = L * xi[sec];
  double Nv1 = x * (1 - x / L)*(1 - x / L);
  double Nv2 = x * x / L * (x / L - 1);
  double Nw1 = -Nv1;
  double Nw2 = -Nv2;

  if (geomLinear) {
	  Nd1(0, 0) = 1.0;
	  Nd1(1, 1) = x / L - 1.0;
	  Nd1(1, 2) = x / L;
	  Nd1(2, 3) = x / L - 1.0;
	  Nd1(2, 4) = x / L;
	  Nd1(3, 6) = 1.0;
	  Nd1(4, 5) = 1.0;
  } else {
	  Nd1(0, 0) = 1.0;
	  Nd1(1, 0) = Nv1 * v(1) + Nv2 * v(2);
	  Nd1(1, 1) = x / L - 1.0;
	  Nd1(1, 2) = x / L;
	  Nd1(2, 0) = -Nw1 * v(3) - Nw2 * v(4);
	  Nd1(2, 3) = x / L - 1.0;
	  Nd1(2, 4) = x / L;
	  Nd1(3, 6) = 1.0;
	  Nd1(4, 5) = 1.0;
  }

  return Nd1;
}
/*
void mixedBeamColumn3d::getSectionTangent(int sec,int type,Matrix &kSection,
                                          double &GJ) {
  int order = sections[sec]->getOrder();
  const ID &code = sections[sec]->getType();

  // Initialize formulation friendly variables
  kSection.Zero();
  GJ = 0.0;

  // Get the stress resultant from section
  Matrix sectionTangent(order,order);
  if ( type == 1 ) {
    sectionTangent = sections[sec]->getSectionTangent();
  } else if ( type == 2 ) {
    sectionTangent = sections[sec]->getInitialTangent();
  } else {
    sectionTangent.Zero();
  }

  // Set Components of Section Tangent
  int i,j;
  for (i = 0; i < order; i++) {
    for (j = 0; j < order; j++) {
      switch(code(i)) {
        case SECTION_RESPONSE_P:
          switch(code(j)) {
            case SECTION_RESPONSE_P:
              kSection(0,0) = sectionTangent(i,j);
              break;
            case SECTION_RESPONSE_MZ:
              kSection(0,1) = sectionTangent(i,j);
              break;
            case SECTION_RESPONSE_MY:
              kSection(0,2) = sectionTangent(i,j);
              break;
            default:
              break;
          }
          break;
        case SECTION_RESPONSE_MZ:
          switch(code(j)) {
            case SECTION_RESPONSE_P:
              kSection(1,0) = sectionTangent(i,j);
              break;
            case SECTION_RESPONSE_MZ:
              kSection(1,1) = sectionTangent(i,j);
              break;
            case SECTION_RESPONSE_MY:
              kSection(1,2) = sectionTangent(i,j);
              break;
            default:
              break;
          }
          break;
        case SECTION_RESPONSE_MY:
          switch(code(j)) {
            case SECTION_RESPONSE_P:
              kSection(2,0) = sectionTangent(i,j);
              break;
            case SECTION_RESPONSE_MZ:
              kSection(2,1) = sectionTangent(i,j);
              break;
            case SECTION_RESPONSE_MY:
              kSection(2,2) = sectionTangent(i,j);
              break;
            default:
              break;
          }
          break;
        case SECTION_RESPONSE_T:
          GJ = sectionTangent(i,i);
          break;
        default:
          break;
      }
    }
  }
}

void mixedBeamColumn3d::getSectionStress(int sec,Vector &fSection,
                                         double &torsion) {
  int order = sections[sec]->getOrder();
  const ID &code = sections[sec]->getType();

  // Get the stress resultant from section
  Vector stressResultant = sections[sec]->getStressResultant();

  // Initialize formulation friendly variables
  fSection.Zero();
  torsion = 0.0;

  // Set Components of Section Stress Resultant
  int j;
  for (j = 0; j < order; j++) {
    switch(code(j)) {
      case SECTION_RESPONSE_P:
        fSection(0) = stressResultant(j);
        break;
      case SECTION_RESPONSE_MZ:
        fSection(1) = stressResultant(j);
        break;
      case SECTION_RESPONSE_MY:
        fSection(2) = stressResultant(j);
        break;
      case SECTION_RESPONSE_T:
        torsion = stressResultant(j);
        break;
      default:
        break;
    }
  }
}

void mixedBeamColumn3d::setSectionDeformation(int sec,Vector &defSection,
                                              double &twist) {
  int order = sections[sec]->getOrder();
  const ID &code = sections[sec]->getType();

  // Initialize Section Deformation Vector
  Vector sectionDeformation(order);
  sectionDeformation.Zero();

  // Set Components of Section Deformations
  int j;
  for (j = 0; j < order; j++) {
    switch(code(j)) {
      case SECTION_RESPONSE_P:
        sectionDeformation(j) = defSection(0);
        break;
      case SECTION_RESPONSE_MZ:
        sectionDeformation(j) = defSection(1);
        break;
      case SECTION_RESPONSE_MY:
        sectionDeformation(j) = defSection(2);
        break;
      case SECTION_RESPONSE_T:
        sectionDeformation(j) = twist;
        break;
      default:
        break;
    }
  }

  // Set the section deformations
  int res = sections[sec]->setTrialSectionDeformation(sectionDeformation);
}
*/

int mixedBeamColumn3d::sendSelf(int commitTag, Channel &theChannel) {
  // @todo write mixedBeamColumn3d::sendSelf
  opserr << "Error: mixedBeamColumn3d::sendSelf -- not yet implemented for mixedBeamColumn3d element";
  return -1;
}

int mixedBeamColumn3d::recvSelf(int commitTag, Channel &theChannel,
                                FEM_ObjectBroker &theBroker) {
  // @todo write mixedBeamColumn3d::recvSelf
  opserr << "Error: mixedBeamColumn3d::sendSelf -- not yet implemented for mixedBeamColumn3d element";
  return -1;
}
