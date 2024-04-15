//
// Geiger Counter enclosure
//
// A quick&dirty hack released "as-is"
//
// (c) 2024 Dan Julio
//
include<radioactive_trefoil.scad>;


// Render settings
//  1. Base (for printing)
//  2. Top (for printing)
//  3. Switch base (for printing)
//  4. Button (for printing)
//  5. Radioactive trefoil (for printing)
//  6. All parts together
//
rend = 6;

l = 190;
w = 40;
h = 20;
ht = 7;
tref_wid = 22;


module button()
{
    cylinder(h=3, d=4, $fn=120);
    cylinder(h=1, d=6, $fn=120);
}


module button_hole()
{
    // Hole
    translate([0, 0, -0.1])
    {
        cylinder(h=2.2, d=4.2, $fn=120);
    }
    
    // Base capture in bezel
    translate([0, 0, 1])
    {
        cylinder(h=1.1, d=6.2, $fn=120);
    }
}


module tube_standoff(bh)
{
//    cube([19, 30, bh]);
    
    difference()
    {
        union()
        {
            cube([19, 2, bh+7.4]);
            
            translate([0, 28, 0])
            {
                cube([19, 2, bh+7.4]);
            }
        }
        
        translate([19/2, -0.1, bh+7.5])
        {
            rotate([-90, 0, 0])
            {
                cylinder(h=30.2, r=7.6, $fn=120);
            }
        }
    }
}


module bottom()
{
    difference()
    {
        union()
        {
            // Main body
            difference()
            {
                intersection()
                {
                    cube([w, l, h]);
                    translate([w/2, 0, w/2])
                    {
                        rotate([-90, 0, 0])
                        {
                            cylinder(h=l, d=1.075*w, $fn=120);
                        }
                    }
                }
            
                // Inner cutout
                translate([w/2, 2, 0.475*w])
//              translate([w/2, -0.1, 0.475*w])
                {
                    rotate([-90, 0, 0])
                    {
                        cylinder(h=l-4, d=0.9*w, $fn=120);
//                      cylinder(h=l+0.2, d=0.9*w, $fn=120);
                    }
                }
            }
            
            // Inner base
            intersection()
            {
                union()
                {
                    translate([0.2*w, 0, 0])
                    {
                        cube([0.6*w, l, 0.25*h]);
                    }
                    
                    // Tube mount
                    translate([(w-19)/2, 8, 0])
                    {
                        tube_standoff(6.5);
                    }
                    
                    // Detector PCB standoffs
                    translate([(w-22)/2, 65, 0])
                    {
                        cube([2, 30, 10.5]);
                    }
                    translate([(w+22)/2 - 2, 65, 0])
                    {
                        cube([2, 30, 10.5]);
                    }
                    
                    // Switch standoffs
                    translate([(w-20)/2, 105, 0])
                    {
                        cube([5, 25, 4 + 0.25*h]);
                    }
                    translate([(w+20)/2 - 5, 105, 0])
                    {
                        cube([5, 25, 4 + 0.25*h]);
                    }
                    
                    // Battery case mount 
                    translate([w/2 - 1, 145, 0])
                    {
                        cube([2, 30, 12 + 0.25*h]);
                    }
                }
                
                translate([w/2, 0, w/2])
                {
                    rotate([-90, 0, 0])
                    {
                        cylinder(h=l, d=1.075*w, $fn=120);
                    }
                }
            }
            
            // Mounting flanges
            translate([0, 0, h])
            {
                difference()
                {
                    cube([w, l, 1]);
                    
                    translate([1, 1, -0.1])
                    {
                        cube([w-2, l-2, 1.2]);
                    }   
                }
            }
        }
        
        // Geiger end cutout
        translate([w/2, -0.1, 14])
        {
            rotate([-90, 0, 0])
            {
                cylinder(h=2.2, d=11, $fn=120);
            }
        }
    }
}


module top()
{
    difference()
    {
        union()
        {
            // Base
            difference()
            {
                // Base
                intersection()
                {
                    cube([w, l, ht]);
                    
                    translate([w/2, 0, 0.3*w])
                    {
                        rotate([-90, 0, 0])
                        {
                            cylinder(h=l, d=1.05*w, $fn=120);
                        }
                    }
                }
                
                // Internal cutout
                translate([2.5, 2, 2])
                {
                    cube([w-5, l-4, ht]);
                }
            }
            
            // Mounting flanges
            difference()
            {
                translate([1.1, 1.1, ht])
                {
                    cube([w-2.2, l-2.2, 1]);
                }
                
                translate([2, 2, ht-0.1])
                {
                    cube([w-4, l-4, 1.2]);
                }
            }
            
            // Tube mount
            translate([(w-19)/2, 8, 0])
            {
                tube_standoff(5.5);
            }
            
            // Switch standoffs
            /*
            translate([(w-30)/2, 115, 0])
            {
                cube([3, 10, 4 + 13.4]);
            }
            translate([(w+30)/2 - 3, 115, 0])
            {
                cube([3, 10, 4 + 13.4]);
            }
            */
        }
        
        // Switch cutout
        translate([(w-18)/2, 113, -0.1])
        {
            cube([18, 11, 2.2]);
        }
        
        // LCD cutout
        translate([(w-19)/2, 141, -0.1])
        {
            cube([19, 33, 2.2]);
        }
        
        // Button cutouts
        translate([(w-17)/2, 183, 0])
        {
            button_hole();
        }
        translate([(w-17)/2 + 17, 183, 0])
        {
            button_hole();
        }
        
        // USB cutout
        translate([(w-10)/2, 180, 0.5])
        {
            cube([10, (l-180)+0.1, 4]);
        }
        translate([(w-11)/2, l-0.5, -0.1])
        {
            cube([11, 0.6, 5.1]);
        }
    }
}


module switch_standoff()
{
    difference()
    {
        union()
        {
            cube([30, 25, 1]);
            
            cube([4, 25, 4]);
            translate([26, 0, 0])
            {
                cube([4, 25, 4]);
            }
        }
    
    translate([(30-25.5)/2, 3, -0.1])
    {
        cylinder(h=4.2, d=1.5, $fn=120);
    }
    translate([30 - (30-25.5)/2, 3, -0.1])
    {
        cylinder(h=4.2, d=1.5, $fn=120);
    }
    }
}


module trefoil(wid)
{
    linear_extrude(1.1)
    radioactive_trefoil(wid, 30);
}


// =============================================================================
// Render code
//

if (rend == 1)
{
    bottom();
}

if (rend == 2)
{
    top();
}

if (rend == 3)
{
    switch_standoff();
}

if (rend == 4)
{
    button();
}

if (rend == 5)
{
    trefoil(tref_wid);
}

if (rend == 6)
{
    bottom();
    
    translate([w + w/4, 0, 0])
    {
        top();
    }
    
    translate([2*w + w/2, 0, 0])
    {
        switch_standoff();
    }
    
    translate([2*w + 0.55*w, 50, 0])
    {
        button();
    }
    
    translate([3*w + 0.15*w, 50, 0])
    {
        button();
    }
    
    translate([2*w + 0.85*w, 75, 0])
    {
        trefoil(tref_wid);
    }
}
