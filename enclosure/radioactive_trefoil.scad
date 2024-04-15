// Not specifically for the trefoil but named so it does not interfere
// with other arc modules
module radioactive_trefoil_arc(radius_outside, 
           radius_inside, 
           angle_start, 
           angle_end) {
  difference() {
    difference() {
      polygon([
        [0,0], 
        [cos(angle_start) * (radius_outside + 50), 
         sin(angle_start) * (radius_outside + 50)], 
        [cos(angle_end) * (radius_outside + 50), 
         sin(angle_end) * (radius_outside + 50)]]);
        circle(r = radius_inside);
    }
    difference() {
      circle(r=radius_outside + 100);
      circle(r=radius_outside);
    }
  }
}


/**
 * Creates a 2D radioactive trefoil according to
 * International Atomic Energy Agency (IAEA) Standards
 * - inner circle radius = R
 * - inner foil radius = 1.5 R
 * - outer foil radius = 5 R
 * by specifying the outer diameter for more conveniance
 * @param diameter Outer trefoil diameter
 * @param rotation Rotate the trefoil by angle (default: 0)
 */
module radioactive_trefoil(diameter, rotation=0) {
    radius = diameter / 2;
    circle_radius = radius / 5;
    foil_inner_radius = circle_radius * 1.5;

    circle(circle_radius);

    for (inc = [0:1:2])
        radioactive_trefoil_arc(radius, foil_inner_radius, 
        rotation + inc * 120, 
        (rotation + 60) + inc * 120);
}

/**
 * Creates a encircled 2D radioactive trefoil according to
 * International Atomic Energy Agency (IAEA) Standards
 * - inner circle radius = R
 * - inner foil radius = 1.5 R
 * - outer foil radius = 5 R
 * by specifying the outer diameter for more conveniance
 * @param diameter Circle diameter
 * @param distance_circle distance from foil to inner diameter of the circle
 * @param circle_width Width of the circle around the trefoil
 * @param rotation Rotate the trefoil by angle (default: 0)
 */
module radioactive_trefoil_encircled(diameter, distance_circle, circle_width, rotation=0) {
    union() {
        radioactive_trefoil(diameter - distance_circle * 2 - circle_width * 2, rotation);
        difference() {
            circle(d=diameter);
            circle(d=diameter - circle_width * 2);
        }
    }
}
