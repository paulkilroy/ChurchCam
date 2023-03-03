/*
////////////////////////////////////////////////////
See my other designs:
https://www.thingiverse.com/steeveeet/designs
////////////////////////////////////////////////////

This file is designed to be entirely customisable.

The easiest way to use this is to copy the top sections (Basic and Advanced Customisation) into a new file
and add `include <U_Box.scad>;` at the top of that file. That will bring all the functionality of this file into a new file
so that you can define the box specific value for it.

There are 3 main sections to this file:

*** Basic Customisation ***
Top section defines the variables for the box - 
these are all exposed through the Customizer UI.
The last section (display settings) is used to control what is displayed 
(and thus exported to STL when the time comes)


*** Advanced Customisation ***
The configuratino fo the end panels are here.
These modules define the shapes to use to cut out holes from the front and back panels,
and to add (text) decoration to the front and back panels

In the PCB mount locations you'll find an array of point constructed 
from the rectangle defined by the PCB Width and Height defined in the UI.
If you wish to ovverride this (if you need more than 4 mounting points, 
or they are not in a rectangle) simply construct that array in directly and the model will update


*** Implementation ***
This section contains the modules and functions that are used to actually construict the parts of the box
It's unlikely you will need to change this, though I'd be very interested if you find that you do!

////////////////////////////////////////////////////
*/

/********************************************************************
***                      Basic Customisation                      ***
*********************************************************************/

/* [Case Settings] */
    Length = 130;
    Width = 80;
    Height = 60;
    WallThickness = 2.5; // [1:5]  

    FilletRadius = 3; // [0.1:12] 
    // Decorations or ventilation holes
    VentType = "None"; // [None, Decorations, VentHoles:Ventilation Holes]
    // Width of vent/decoration holes
    VentWidth = 1.5;

/* [Fastening Options] */
    // Methods of securing the closure of the box
    FasteningType = "Screw"; // [Screw:Screw Fit, Push:Push Fit]
    // Number for fastenings along thge edge
    NumberOfFastenings = 2; // [0:10]
    // Thickness of the fastening tabs
    FasteningTabThickness = 2; // [1:5]
    // Diameter of fastener hole for screw fitting
    // PSK was 2.0
    FasteningHoleDiameter = 2.2; // [0.1:5]

/* [End Panels] */
    BackPanelType = "Discrete"; // [None, IntegratedWithTop:Integrated With Top, IntegratedWithBase:Integrated With Base, Discrete]
    FrontPanelType = "Discrete"; // [None, IntegratedWithTop:Integrated With Top, IntegratedWithBase:Integrated With Base, Discrete]
    // Panel Text Type
    EmbossPanelDecorations = 1; // [0:Demboss, 1:Emboss]
    // End Panel Tolerance PSK was .9 - .3 a little too tight
    PanelTolerance = 0.4;
    // Use example panel decorations
    examplePanels = 0; // [0:No, 1:Yes]


/* [PCB Mounts] */
// https://github.com/OLIMEX/ESP32-POE-ISO/blob/master/HARDWARE/Dimensions/ESP32-POE-ISO-revision-J-dimensions.pdf

    // Show PCB Ghost
    ShowPCB = 1; // [0:No, 1:Yes]
    PCBHolePadding = 2.5654;

    // Eth jack sticks out 8mm out -- but want it flush with the panel
    EthLPro = 8;
    EthWPro = 1.21;

    // Add PCB Mounts
    PCBMounts = 1; // [0:No, 1:Yes]
    // Back left corner X position
    // PSK this is relative to the interion of the box, which includes in slot holder for the panels
    // need to back it up 
    PCBPosX = 0;//EthLPro; 
    // Back left corner Y position
    PCBPosY = WallThickness*2;
    // PCB Length
    PCBLength = 70.6750 + PCBHolePadding;
    
    PCBThickness = 1.5;
    // PCB Width
    PCBWidth = 28;
    PCBHoleSeparation = 22.86;
    
    // PSK I only need back feet (I think)
    pcbMountLocations = [  [PCBHolePadding,PCBLength-PCBHolePadding], [PCBWidth-PCBHolePadding,PCBLength-PCBHolePadding] ];
    
    // Mount height
    MountHeight = 5;
    // Mount diameter
    MountDia = 5;
    // Hole diameter
    MountHoleDia = 1.7; // PSK 2.5;
    
    //Ethernet Jack: http://www.link-pp.com/upload/file/20171110/20171110115241054105.pdf
    EthWidth = 17;
    EthHeight = 14;
    EthLength = 21.19;
    EthZPos = MountHeight + PCBThickness;
    
    
    OScreenWidth = 29.42+2;
    OScreenLength = 14.7+2;
    OScreenCenterX = -Length/2+28;
    OScreenCenterY = 0;
    OHole1X = OScreenCenterX + 14.7/2 + 6.2 + 5.25 - 2.5;    // Lower Left
    OHole1Y = OScreenCenterY - 29.42/2 - 2.54 - .45 + 2.5;   // Lower Left
    OHole2X = OHole1X;   // Lower Right
    OHole2Y = OHole1Y + 30.4;   // Lower Right
    OHole3X = OHole1X - 28.5;   // Upper Left
    OHole3Y = OHole1Y;   // Upper Left
    OHole4X = OHole2X - 28.5;   // Upper Left
    OHole4Y = OHole2Y;   // Upper Left

/* [Display Settings] */
    // Print Layout
    PrintLayout = 1; // [0:No, 1:Yes]
    // Top Case
    ShowTCase = 1; // [0:No, 1:Yes]
    // Bottom Case
    ShowBCase = 1; // [0:No, 1:Yes]
    // Front panel
    ShowFPanel = 1; // [0:No, 1:Yes]
    // Back panel  
    ShowBPanel = 1; // [0:No, 1:Yes]

/* [Hidden] */
    // Case color  
    CaseColour = "Teal";
    // End Panel color    
    PanelColour = "Aqua";
    // Fillet Smootheness  
    Resolution = 50; // [4:100] 


/************************************************************
***                   Advanced Customisation             ***
************************************************************/
{
    //////////////////////////////////////////
    /////////// Panel Configuration //////////
    //////////////////////////////////////////

    module FrontPanelCutouts(){
        if (examplePanels)
        {
            SquareHole  ([10,5],[10,10],1);
            CylinderHole([40,10],5);
        }

    }

    module FrontPanelDecoration(){
        if (examplePanels)
        {
            LText([7.5,17.5], "On/Off");
            CText([40,10], 5, "12345");
        }
    }

    module BackPanelCutouts(){
        if (examplePanels)
        {
            SquareHole  ([5,0],[10,10],1);
            CylinderHole([10,10],10);
        }
        
        USBWidth = 10;
        USBHeight = 5; // Unknown?
        USBHoleDiam = 3; // Unknown?
        USBHoleSep = 17; 
        SquareHole  ([Width/2-20-USBWidth/2-WallThickness, 7],[USBWidth,USBHeight],0);
        
        CylinderHole([Width/2-20-USBHoleSep/2-WallThickness, 7+USBHeight/2], USBHoleDiam);
        CylinderHole([Width/2-20+USBHoleSep/2-WallThickness, 7+USBHeight/2], USBHoleDiam);

        //translate(-InteriorDims()/2 + [PCBPosX, PCBPosY, 0]){
        echo("PCBPosY-EthWPro: ", PCBPosY-EthWPro);
        echo("Width: ", Width);
        SquareHole  ([Width-WallThickness*2-EthWidth-PCBPosY-EthWPro-.5,EthZPos-.5],[EthWidth,EthHeight],0);
        //}
    }

    module BackPanelDecoration(){
        if (examplePanels)
        {
            LText([17.5,5], "Power");
        }
    }

    //////////////////////////////////////////
    /////////// PCB Mount locations //////////
    //////////////////////////////////////////

    //pcbMountLocations = [[0,0], [PCBWidth,0], [0,PCBLength], [PCBWidth,PCBLength] ];
    


}

/************************************************************
***                   Implementation                      ***
************************************************************/
{
    //////////////////////////////
    /////////// Helpers //////////
    //////////////////////////////

    $fn = Resolution;

    ////////// CONSTANT: Thickness of end panels //////////
    function PanelThickness() = WallThickness;

    ////////// CONSTANT: Helper to define how much space is needed for end panel ribs, depending on type //////////
    function SpaceForEndPanel(type) = 
        type == "IntegratedWithTop" ? PanelThickness() : 
        type == "IntegratedWithBase" ? PanelThickness() : 
        type == "Discrete" ? 2*WallThickness + PanelThickness() + PanelTolerance : 
        0; // None

    ////////// CONSTANT: How much of the length of the case will be taken up by support ribs for the end panels //////////
    function SpaceForEndPanels() = 2*max(SpaceForEndPanel(FrontPanelType), SpaceForEndPanel(BackPanelType));

    ////////// CONSTANT: Vector describing exterior dimensions of the case //////////
    function CaseDims() = [Length, Width, Height];

    ////////// CONSTANT: The clear space before fittings for end panels //////////
    function InteriorDims() = [Length - SpaceForEndPanels(), Width - 2*WallThickness, Height - 2*WallThickness];

    ////////// Calculated size of tabs. Default to 8mm, but reduce if a small length or height //////////
    function TabDiameter() = 
        min(Height/2, // Don't collide with Vent Holes in reduced height boxes
            min(InteriorDims().x/2, // Don't collide with other tabs in reduced length boxes
                20)); // Sensible max height


    //////////////////////////////////////////////////
    /////////// Maths functions & Utilities //////////
    //////////////////////////////////////////////////


    ////////// Utility to work out how many items you might fit ion a given length //////////
    function numItemsAlongLength(length, itemSpacing) = ceil(length / itemSpacing);

    // Helper functions to calculate the bounding box of an array of vec2s
    function bbox(v) = [minimum(v), maximum(v)];
    {
        // Return the maximum of all the "elem"th elemtns in a vector of vectors
        function minimumElem(v, elem, i = 0) = 
            (i < len(v) - 1) ? 
                min(v[i][elem], minimumElem(v, elem, i+1)) : 
                v[i][elem];

        // Return the maximum of all the "elem"th elemtns in a vector of vectors
        function maximumElem(v, elem, i = 0) = 
            (i < len(v) - 1) ? 
                max(v[i][elem], maximumElem(v, elem, i+1)) : 
                v[i][elem];

        function minimum(v) = 
            [minimumElem(v, 0), minimumElem(v, 1)];
        function maximum(v) = 
            [maximumElem(v, 0), maximumElem(v, 1)];
    }

    /////  Operator: distribute copies of the children evenly along a vector //////////
    module distribute(start, end, numItems){
        vec = end-start;
        length = norm(vec);
        normVec = vec/length;

        itemSpacing = length / (2*numItems);
        
        // Even number of tabs
        for (i=[0: 1: numItems-1])
        {
            tabLoc = start + (itemSpacing * (1 + 2*i)) * normVec;

            translate([tabLoc.x, tabLoc.y, tabLoc.z])
            {
                children();
            }                    
        }
    }

    /////////// Generic rounded box - Aligned along the X axis //////////
    module RoundBox(dims, fillet = 0, center = false){
        if (fillet > 0){
            // Calculate the correct offset depending on whether the cube should be offset

            // Split the total height between cubeDims & cylHeight
            cylHeight = dims.x/2;
            // The fillet radius must (at most) be half either Y, or Z, but we cannot have cubeDims Y or Z equal to zero
            cappedFillet = min(min(fillet, dims.y/2.001), dims.z/2.001);

            cubeDims = [
                dims.x/2,
                dims.y - 2*cappedFillet,
                dims.z - 2*cappedFillet,
            ];

            offset = center ? [0,0,0] : dims/2;
            translate(offset){
                minkowski()
                {
                    rotate([0, 90, 0]){
                        cylinder(r = cappedFillet, h = cylHeight, center = true);
                    }
                    cube(cubeDims, center = true);
                }
            }
        } else {
            cube(dims, center = center);
        }
    }// End of RoundBox Module

    /////////// Generic hollow rounded box - Aligned along the X axis //////////
    module HollowRoundBox(dims, thickness, fillet = 0, center = false){
        offset = center ? [0,0,0] : dims/2;
        translate(offset){
            difference() {
                RoundBox(dims, fillet, true);
                RoundBox([dims.x * 1.1, dims.y - 2*thickness, dims.z - 2*thickness], fillet, true);
            }
        }
    }// End of RoundBox Module

    ////////////////////////////////////////////////////////
    ////////////////////// Main Case ///////////////////////
    ////////////////////////////////////////////////////////

    module PanelRibs(panelType, xMultiplier = 1)
    {
        // For discrete panels, define the ribs to hold them in place
        discreteRibDims = [WallThickness, CaseDims().y - 2*WallThickness, CaseDims().z - 2*WallThickness];
        // Effective thickness of the discrete panel, including tolerance
        discretePanelThickness = PanelTolerance + WallThickness;
        // Location for slot for discrete panels
        slotLocation = CaseDims().x/2 - (discreteRibDims.x + discretePanelThickness/2);
        // Offset (from slotLocation) for retaining ribs
        slotOffset = (discreteRibDims.x + discretePanelThickness)/2;

        // For integrated panels, the rib can be smaller
        integratedRibDims = [WallThickness/2, CaseDims().y - 2*WallThickness, CaseDims().z - 2*WallThickness];
        // For integrated panels, the rib can be smaller
        integratedRibOffset = (WallThickness + PanelTolerance/2) + integratedRibDims.x/2;

        // Front Panel
        if(panelType == "Discrete")
        {
            // Add ribs for supporting discrete panel
            translate([xMultiplier * (slotLocation + slotOffset), 0, 0]){
                HollowRoundBox(discreteRibDims, WallThickness, FilletRadius, true);
            }
            translate([xMultiplier * (slotLocation - slotOffset), 0, 0]){
                HollowRoundBox(discreteRibDims, WallThickness, FilletRadius, true);
            }
        }
        else if(panelType == "IntegratedWithThis"){
            // Add a supporting rib for the panel integrated with this half
            translate([xMultiplier * (CaseDims().x/2 - WallThickness/2), 0, 0]){
                HollowRoundBox([WallThickness, CaseDims().y - 2*WallThickness, CaseDims().z - 2*WallThickness], PanelTolerance/2, FilletRadius, true);
            }
        }
        else if(panelType == "IntegratedWithOther"){
            // Add a supporting rib for the panel integrated with the other half
            translate([xMultiplier * (CaseDims().x/2 - integratedRibOffset), 0, 0]){
                HollowRoundBox(integratedRibDims, WallThickness/2, FilletRadius, true);
            }
        }
    }

    ////////////////////////////////// Case //////////////////////////////////
    module Case(dims, thickness, fillet = 0, fPanelType = "None", bPanelType = "None"){
        // Hull with integrated panels (where appropriate)
        union(){
            // Decorated hull
            difference()
            {
                // Main hull
                union() {

                    // Main Shell 
                    HollowRoundBox(dims, thickness, fillet, true);

                    // Add ribs for supporting the end panels
                    PanelRibs(fPanelType, 1);
                    PanelRibs(bPanelType, -1);
                }
                // Remove the top
                translate([0, 0, Height / 2]){
                    cube([dims.x*1.1, dims.y*1.1, dims.z], true);
                }

                // Add side decoration / vents
                if (VentType != "None")
                {
                    union()
                    {
                        overlap = VentWidth; //make sure that we cleanly overlap the top/bottom/side of the case
                        decDims = [
                            VentType == "VentHoles" ? 
                                (thickness + fillet) + overlap : 
                                thickness/2 + overlap,
                            VentWidth,
                            (dims.z / 4) + overlap
                        ];
                        padding = dims.x/20; // (half) the gap between sets of vents (and between vents and end)
                        ventFillet = VentWidth/3;

                        // Coords for vents
                        xStart = padding;
                        xEnd  = InteriorDims().x/2 - padding;
                        yPos = VentType == "VentHoles" ? 
                            InteriorDims().y/2 + decDims.x/2 - fillet : // Calculated from the inside wall
                            CaseDims().y/2 - decDims.x/2 + overlap; // Calculated from the ouside wall
                        zPos = -0.5*(dims.z - decDims.z) - overlap;

                        numVents = numItemsAlongLength(xEnd-xStart, VentWidth+thickness);

                        distribute([xStart, yPos, zPos], [xEnd, yPos, zPos], numVents)
                        {
                            rotate([0, 0, 90])
                                RoundBox(decDims, ventFillet, true);
                        }
                        distribute([-xStart, yPos, zPos], [-xEnd, yPos, zPos], numVents)
                        {
                            rotate([0, 0, 90])
                                RoundBox(decDims, ventFillet, true);
                        }
                        distribute([xStart, -yPos, zPos], [xEnd, -yPos, zPos], numVents)
                        {
                            rotate([0, 0, 90])
                                RoundBox(decDims, ventFillet, true);
                        }
                        distribute([-xStart, -yPos, zPos], [-xEnd, -yPos, zPos], numVents)
                        {
                            rotate([0, 0, 90])
                                RoundBox(decDims, ventFillet, true);
                        }
                    }
                }
            }
        }
    }

    ////////////////////////////////// Fixing tabs //////////////////////////////////
    module Tab(radius, thickness){
        difference(){
            union(){
                rotate([90, 0, 0])
                {                
                    cylinder(r = radius, thickness, $fn = 6);
                }
            }
            // Chamfer
            translate([0, -radius/2, -2*radius/3]){
                rotate([45, 0, 0]){
                    cube([2*radius, radius, radius], center=true);
                }
            }

            // Tolerance cut
            tolerance = 0.1;
            tolBox = [2*radius, 2*thickness, radius];
            translate([0, (thickness-tolerance), tolBox.z/2]){
                cube(tolBox, center=true);
            }
        }
    }

    ////////////////////////////////// Case with tabs //////////////////////////////////
    module CaseWithTabs(dims, thickness, fillet = 0, fPanelType = "None", bPanelType = "None"){
        cylRad = TabDiameter()/2;
        holeDia = FasteningHoleDiameter;

        // Calculate the number of tabs and their location based on the length of the box.
        // Assume a gap of TabDiameter on each side of each tab before spawning a new tab
        numItems = NumberOfFastenings;

        // For push fitting, offset the registration sphere so it's not a complete hemisphere. Makes it easier to close.
        // Setting this to zero will result in a complete hemisphere
        sphereOffset = 0.125*holeDia;

        difference(){
            union(){
                Case(dims, thickness, fillet, fPanelType, bPanelType);

                // Tabs
                distribute(
                    [InteriorDims().x/2, InteriorDims().y/2, 0], 
                    [-InteriorDims().x/2, InteriorDims().y/2, 0],
                    numItems)
                {
                    Tab(cylRad, FasteningTabThickness);
                }

                if (FasteningType == "Push"){
                    // Add registration spheres
                    distribute(
                        [InteriorDims().x/2, -InteriorDims().y/2 - sphereOffset, -cylRad/3],
                        [-InteriorDims().x/2, -InteriorDims().y/2 - sphereOffset, -cylRad/3],
                        numItems)
                    {
                        difference(){
                            sphere(d = holeDia);
                            // Ensure the sphere doesn't stick out of the other side of the wall!
                            translate([0,-(holeDia - 2.*sphereOffset)/2,0])
                            {
                                cube(holeDia, true);
                            }
                        }
                    }
                }
            }

            if (FasteningType == "Push"){
                // Add cutouts for registration spheres
                distribute(
                    [InteriorDims().x/2, InteriorDims().y/2 + sphereOffset, cylRad/3],
                    [-InteriorDims().x/2, InteriorDims().y/2 + sphereOffset, cylRad/3],
                    numItems
                    )
                {
                    sphere(d = holeDia);
                }
            }
            else if (FasteningType == "Screw"){
                // Screw fitting so add the case holes
                distribute(
                    [InteriorDims().x/2, InteriorDims().y/2, cylRad/3],
                    [-InteriorDims().x/2, InteriorDims().y/2, cylRad/3],
                    numItems
                    )
                {
                    rotate([90, 0, 0]){
                        cylinder(d = holeDia, 3.0*thickness, center=true);
                    }
                }
                distribute(
                    [InteriorDims().x/2, -InteriorDims().y/2, -cylRad/3],
                    [-InteriorDims().x/2, -InteriorDims().y/2, -cylRad/3],
                    numItems)
                {
                    // PSK Nudging cylinder out of case a bit
                    translate( [0,-3.5,0])
                       rotate([90, 0, 0]){
                        // PSK Making screw hold a cone to make screws flush
                        // cylinder(d = holeDia, 3.0*thickness, center=true);
                        cylinder(d1 = holeDia, d2 = holeDia*4, 3.0*thickness, center=true);
                    }
                }                
            }        
        }
    }

    /////////////////////////////////////////////////////////
    ////////////////////// PCB Mounts ///////////////////////
    /////////////////////////////////////////////////////////

    /////////////////////// PCB Mount /////////////////////////////
    module PCBMount(){
        FilletRadius = 2;
        color(CaseColour)
        union(){
            difference(){
                // Main cylinder
                cylinder(d = MountDia, MountHeight);
                // central hole
                cylinder(d = MountHoleDia, MountHeight*1.01);

                // Chamfer to help centre screws
                chamferRad = (MountDia-MountHoleDia)/4;
                rotate_extrude(){
                    translate([MountHoleDia / 2, (MountHeight-chamferRad)*1.01, 0]){
                        difference(){
                            square(chamferRad);
                            translate([chamferRad,0,0])
                                circle(chamferRad, $fn=4);
                        }
                    }
                }
            }

            // Fillet
            rotate_extrude(){
                translate([MountDia / 2, 0, 0]){
                    difference(){
                        square(FilletRadius);
                        translate([FilletRadius,FilletRadius,0])
                            circle(FilletRadius);
                    }
                }
            }
        }
    }


    module PCBMounts(){
        if (PCBMounts)
        {
            translate(-InteriorDims()/2 + [PCBPosX, PCBPosY, 0]){
                if (ShowPCB)
                {
                    translate([0, 0, MountHeight]){
                        % union() {
                            cube([PCBLength, PCBWidth, PCBThickness]);
                            color("grey") translate( [63.817-9.1/2,0,0])  cube([9.1, PCBWidth, 9.1]);
                            translate([-EthLPro,EthWPro,PCBThickness]) cube([EthLength, EthWidth, EthHeight]);
                        }
                    }
                    // PCB Ghost
                    //bbox = bbox(pcbMountLocations);
                    //w = bbox[1][0] - bbox[0][0];
                    //l = bbox[1][1] - bbox[0][1];
                    //translate(-[pcbGhostPadding, pcbGhostPadding, -(MountHeight)]){
                        // PSK Changed from square to Cube for knowing eth location
                        //% cube([l + 2*pcbGhostPadding, w + 2*pcbGhostPadding, PCBThickness]);
                    //}
                }

                for(footLocation = pcbMountLocations)
                {
                    translate([footLocation.y, footLocation.x, 0])
                    {
                        PCBMount();
                    }
                }
            }
        }
    }


    /////////////////////////////////////////////////////////
    ////////////////////// End Panels ///////////////////////
    /////////////////////////////////////////////////////////

    /////////////// Circular hole helper //////////////////////
    // loc = location of centre
    // d = diameter of holes
    module CylinderHole(loc, d){
        translate([loc.x, loc.y, -PanelThickness()*0.1]){
            cylinder(d = d, PanelThickness() * 1.2, $fn = Resolution);
        }
    }

    /////////////// Rectangular hole helper /////////////////////
    // loc = location of bottom left corner
    // sz = dimensions of box
    // f = radius of fillet
    module SquareHole(loc, sz, f){
        translate([loc.x, loc.y, -PanelThickness()*0.1]){
            rotate([-90, -90, 0])
            {
                RoundBox([PanelThickness() * 1.2, sz.x, sz.y], f);
            }
        }
    }

    /////////////// Text helper /////////////////////
    // loc = location of bottom left corner
    // content = text to print
    // sz = size of text
    // ft = font to use 
    module LText(loc, content, sz = min(Width,Height)/10, ft = "Arial Black"){
        translate([loc.x, loc.y, PanelThickness()]){
            linear_extrude(height = 1, center = true){
                text(content, size = sz, font = ft);
            }
        }
    }

    /////////////// Arc Text helper /////////////////////
    // loc = location of the centre of the arc
    // content = text to print
    // rad = radius of arc
    // a0 = Start angle (default 0 - west)
    // a1 = End angle (default 180 - east)
    // sz = size of text
    // ft = font to use
    /////////////////////////////////////////////////////
    module CText(loc, r, content, a0=0, a1=180, sz = min(Width,Height)/10, ft = "Arial Black"){
        angleStep = (a1-a0) / (len(content)-1);
        translate([loc.x, loc.y, PanelThickness()])
        for (i = [0: len(content) - 1] ) {
            rotate([0, 0, 90 - (i * angleStep)])
            translate([0, r, 0]) {
                linear_extrude(height = 1, center = true){
                    text(content[i], font = ft, size = sz, valign = "baseline", halign = "center");
                }
            }
        }
    }


    ////////////////////// End Panel //////////////////////
    module Panel(){
        panelDims = [PanelThickness(), InteriorDims().y - PanelTolerance, InteriorDims().z - PanelTolerance];
        cutout = true;
        difference(){
            color(PanelColour)
            // PSK Add to the radius here to ensure a fit
            RoundBox(panelDims, FilletRadius+2, true);

            // Translate so that bottom left corner of panel is "origin" for controls
            translate(-panelDims/2){
                rotate([90, 0, 90]){
                    children(0);
                }
            }
            if (!EmbossPanelDecorations){
                color(CaseColour){
                    translate(-panelDims/2){
                        rotate([90, 0, 90]){
                            children(1);
                        }
                    }
                }
            }
        }

        if (EmbossPanelDecorations){
            color(CaseColour){
                translate(-panelDims/2){
                    rotate([90, 0, 90]){
                        children(1);
                    }
                }
            }
        }
    }

    /////////////////////////////////////////////////////////////////////////
    ////////////////////// Main Scene Build /////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    printLayoutPadding = 10;

    // Utility function to establish whether the panel is integated with This half or the Other
    function panelType(type, this, other) = 
        type == this ? "IntegratedWithThis" :
        type == other ? "IntegratedWithOther" :
        type;

    // Utility to work out the offset from the edge of the case depending on panel type.
    function panelOffset(type) = 
        type == "Discrete" ?
            (WallThickness + PanelThickness()/2 + PanelTolerance/2) :  // Discrete
            PanelThickness()/2; // Integrated (or None, in which case the translate doesn't matter)

    ///////////////////////// Calculate the transforms for each part, depening on whether we're visualising or printing /////////////////////////
    BaseTranslate = !PrintLayout ? 
        CaseDims() / 2: 
        CaseDims() / 2;
    BaseRotate = !PrintLayout ?
        [0,0,0]: 
        [0,0,0];


    TopTranslate = !PrintLayout ? 
        [CaseDims().x/2, CaseDims().y/2, CaseDims().z/1.95] : 
        [CaseDims().x/2, CaseDims().y*1.5 + printLayoutPadding, CaseDims().z/2];
    TopRotate = !PrintLayout ? 
        [180, 0, 0] : 
        [0,0,0];


    FPanelTranslate = (!PrintLayout || FrontPanelType != "Discrete") ? 
        FrontPanelType == "IntegratedWithTop" ?
            TopTranslate + [CaseDims().x/2 - panelOffset(FrontPanelType) - 0.01, 0, 0] : 
            BaseTranslate + [CaseDims().x/2 - panelOffset(FrontPanelType) - 0.01, 0, 0] : 
        [CaseDims().x  + CaseDims().z/2 + printLayoutPadding, CaseDims().y/2, PanelThickness()/2];
    FPanelRotate = (!PrintLayout || FrontPanelType != "Discrete") ? 
        FrontPanelType == "IntegratedWithTop" ?
            TopRotate + [180,0,0]: 
            BaseRotate + [0,0,0]: 
        [0,-90,0];


    BPanelTranslate = (!PrintLayout || BackPanelType != "Discrete") ? 
        BackPanelType == "IntegratedWithTop" ?
            TopTranslate + [-(CaseDims().x/2 - panelOffset(BackPanelType) - 0.01), 0, 0] : 
            BaseTranslate + [-(CaseDims().x/2 - panelOffset(BackPanelType) - 0.01), 0, 0] : 
        [CaseDims().x  + CaseDims().z/2 + printLayoutPadding, CaseDims().y*1.5 + printLayoutPadding, PanelThickness()/2];
    BPanelRotate = (!PrintLayout || BackPanelType != "Discrete") ? 
        BackPanelType == "IntegratedWithTop" ?
            TopRotate + [180,0,180]: 
            BaseRotate + [0,0,180]: 
        [0,-90,0];


    PCBTranslate = !PrintLayout ? CaseDims() / 2 : [0,0,0];
    PCBRotate = !PrintLayout ? [0,0,0]: [0,0,0];

    if (ShowTCase)
    {
      difference() {
        union()
        {

            
            translate(TopTranslate){
                rotate(TopRotate){
                    // Joystick - https://www.micros.com.pl/mediaserver/ryc-rys.r300.jpg
                    translate([0+10,0,-Height/2-39.5-19+WallThickness]) %cylinder(39.5, d=35);
                    translate([0+10,0,-Height/2-19+WallThickness]) %cylinder(19, d=10);
                    translate([0+10,0,0-Height/2+25.5/2+WallThickness])%cube([40.4, 40.4, 25.5],true);
                    color(CaseColour){
                      difference() {
                        union() {
                          // PSK Nubs for the OLED holes
                          translate([OHole1X, OHole1Y, -Height/2+2]) cylinder(8, d=2.8);
                          translate([OHole2X, OHole2Y, -Height/2+2]) cylinder(8, d=2.8);
                          translate([OHole3X, OHole3Y, -Height/2+2]) cylinder(8, d=2.8);
                          translate([OHole4X, OHole4Y, -Height/2+2]) cylinder(8, d=2.8);
                          CaseWithTabs(CaseDims(), WallThickness, FilletRadius,
                            fPanelType = panelType(FrontPanelType, "IntegratedWithTop", "IntegratedWithBase"), 
                            bPanelType = panelType(BackPanelType, "IntegratedWithTop", "IntegratedWithBase"));     
                        }
                        // PSK Holes for the buttons, joystick and lcd
                        // The box is centered at 0,0,0, upside down. So 0,0 is the center 
                        // of the box x and y and -Height/2 is the top edge
                        // Joystick
                        translate([0+10,0,-Height/2-10]) cylinder(20, d=37);
                        translate([0+10-32.5/2,0-32.5/2,-Height/2-10]) cylinder(20, d=3);
                        translate([0+10-32.5/2,0+32.5/2,-Height/2-10]) cylinder(20, d=3);
                        translate([0+10+32.5/2,0-32.5/2,-Height/2-10]) cylinder(20, d=3);
                        translate([0+10+32.5/2,0+32.5/2,-Height/2-10]) cylinder(20, d=3);
                        // Buttons
                        translate([Length/2-18,0,-Height/2-10]) cylinder(20, d=13);
                        translate([Length/2-18,Width/4+2,-Height/2-10]) cylinder(20, d=13);
                        translate([Length/2-18,-Width/4-2,-Height/2-10]) cylinder(20, d=13);
                        //Screen
                        translate([OScreenCenterX,OScreenCenterY,-Height/2]) 
                            cube([OScreenLength, OScreenWidth, 20],true);
                      }
                    }
                }
            }
            // TODO: translated (but don't rotate) the panel with the case - also works for print mode
            // means we have to have the panel translation in local coords (at least not for discrete mode)
            if (FrontPanelType == "IntegratedWithTop")
            {
                translate(FPanelTranslate) {
                    rotate(FPanelRotate){
                        Panel(){
                            FrontPanelCutouts();
                            FrontPanelDecoration();
                        }
                    }
                }
            }
            if (BackPanelType == "IntegratedWithTop")
            {
                translate(BPanelTranslate) {
                    rotate(BPanelRotate){
                        Panel(){
                            BackPanelCutouts();
                            BackPanelDecoration();
                        }
                    }
                }
            }
        }
        /*
        // PSK Holes for the buttons, joystick and lcd
        // Joystick
        translate([Length/2+10,Width/2,Height-10]) cylinder(20, d=37);
        translate([Length/2+10-32.5/2,Width/2-32.5/2,Height-10]) cylinder(20, d=3);
        translate([Length/2+10-32.5/2,Width/2+32.5/2,Height-10]) cylinder(20, d=3);
        translate([Length/2+10+32.5/2,Width/2-32.5/2,Height-10]) cylinder(20, d=3);
        translate([Length/2+10+32.5/2,Width/2+32.5/2,Height-10]) cylinder(20, d=3);
        // Buttons
        translate([Length-18,Width/2,Height-10]) cylinder(20, d=13);
        translate([Length-18,Width/4-2,Height-10]) cylinder(20, d=13);
        translate([Length-18,Width/4*3+2,Height-10]) cylinder(20, d=13);
        //Screen
         translate([OScreenCenterX,OScreenCenterY,Height-1]) cube([OScreenLength, OScreenWidth, 20],true);
*/

          //%translate([OScreenCenterX,OScreenCenterY,Height-1]) cube([OScreenLength, OScreenWidth, 20],true);

      } // End of Diff for Top Holes
    }

    if (ShowBCase) {
        union()
        {
            translate(BaseTranslate){
                rotate(BaseRotate){
                    color(CaseColour){
                        CaseWithTabs(CaseDims(), WallThickness, FilletRadius, 
                            fPanelType = panelType(FrontPanelType, "IntegratedWithBase", "IntegratedWithTop"), 
                            bPanelType = panelType(BackPanelType, "IntegratedWithBase", "IntegratedWithTop"));
                    }
                    PCBMounts();
                    
                }
            }
            if (FrontPanelType == "IntegratedWithBase")
            {
                translate(FPanelTranslate) {
                    rotate(FPanelRotate){
                        Panel(){
                            FrontPanelCutouts();
                            FrontPanelDecoration();
                        }
                    }
                }
            }
            if (BackPanelType == "IntegratedWithBase")
            {
                translate(BPanelTranslate) {
                    rotate(BPanelRotate){
                        Panel(){
                            BackPanelCutouts();
                            BackPanelDecoration();
                        }
                    }
                }
            }
        }
    }

    if (FrontPanelType == "Discrete" && ShowFPanel)
    {
        translate(FPanelTranslate) {
            rotate(FPanelRotate){
                Panel(){
                    FrontPanelCutouts();
                    FrontPanelDecoration();
                }
            }
        }
    }

    if (BackPanelType == "Discrete" && ShowBPanel)
    {
        translate(BPanelTranslate) {
            rotate(BPanelRotate){
                Panel(){
                    BackPanelCutouts();
                    BackPanelDecoration();
                }
            }
        }
    }

}
