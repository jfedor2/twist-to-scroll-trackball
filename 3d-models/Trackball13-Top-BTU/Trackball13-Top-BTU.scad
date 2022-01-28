include <BOSL/constants.scad>
use <BOSL/masks.scad>
use <BOSL/shapes.scad>
use <BOSL/transforms.scad>

$fn=50;

// Epsilon value for offsetting coincident face differences.
E = 0.004;

static_bearing_diameter = 2.5;

// Vertical distance to bottom of ball.
ball_offset_z = 11.4;
ball_diameter = 57.2;
ball_clearance = static_bearing_diameter * 0.25; // 0.625

// Top part of BTU from bottom side of large rim to top of bearing ball.
h_btu_top = 4.8;
d_btu_top = 17;
d_btu_top_clearance = 1;

// Bottom part of BTU that friction fits in the socket.
h_btu_bottom = 6.4;
h_btu_bottom_clearance = 0.2;
d_btu_bottom = 12.6;
d_btu_bottom_clearance = 0.3;

// Wall thickness of BTU socket.
t_btu_socket = 1.2;

// How far to extend the bottom of the top part of the socket past the top of
// the bottom part, So that they are joined together properly like so:
// |      |
// |||  |||   <-- this part
//   |  |
t_btu_rim = 1.2;

// Thickness of cap which is only used for 1 of the BTU sockets.
t_btu_cap = 2;

// BTU angle (from pointing straight up).
a_btu = 60;

// BTU group rotation (around X, Y, Z axes).
btu_group_angle = [0, 0, -30];

// Centre of ball used as reference point for most operations.
ball_centre = [0, 0, ball_offset_z + ball_diameter / 2];

/*import("../Trackball13-Bottom.stl");*/
/*import("../Trackball13-Buttons-Left.stl");*/

difference() {
  import("Trackball13-Top-NoBearingHoles.stl");

  btu_sockets(part="inner");
}

btu_sockets(part="outer");

module btu_sockets(part="inner") {
  for (btu_pos = [-29, 105, 225]) {
    rotate(btu_group_angle) // Set rotation of BTUs as a group.
      rot(a=[a_btu, 0, btu_pos], cp=ball_centre) // Set angle of each BTU.
        translate([0, 0, ball_offset_z]) { // Move to base of ball.
          if (part == "inner")
            btu_socket_inner(); // Drill out hole for BTU socket.

          if (part == "outer") {
            btu_socket_outer(); // Create walls of BTU socket.

            // End cap for this BTU specifically because it sticks outside of
            // the case and we don't want it to look ugly.
            if (btu_pos == 105) {
              translate([0, 0, -(h_btu_top + h_btu_bottom + h_btu_bottom_clearance)])
                difference() {
                  // The cap itself.
                  cyl(
                    h=t_btu_cap,
                    d=d_btu_bottom + d_btu_bottom_clearance + t_btu_socket * 2,
                    fillet1=1,
                    orient=ORIENT_Z,
                    align=V_DOWN,
                  );

                  // Hacky way of cutting off the part of the cap inside the
                  // shell.
                  translate([0, 1, 0])
                    rotate([-49, 0, 0])
                      cyl(h=15, d=30, align=V_FWD);
                }
            }
          }
        }
  }
}

module btu_socket_inner() {
    // Hole for top part of BTU socket.
    zcyl(
      h=h_btu_top * 2 + E,
      d=d_btu_top + d_btu_top_clearance,
      center=true,
    );

    // Hole for bottom part of BTU socket.
    translate([0, 0, -h_btu_top + E])
      zcyl(
        h=h_btu_bottom + h_btu_bottom_clearance,
        d=d_btu_bottom + d_btu_bottom_clearance,
        align=V_DOWN,
      );
}

module btu_socket_outer() {
  difference() {
    // Top part of BTU socket + outer part of rim joining top and bottom.
    tube(
      h=h_btu_top + t_btu_rim,
      ir=(d_btu_top + d_btu_top_clearance) / 2,
      wall=t_btu_socket,
      orient=ORIENT_Z,
      align=V_DOWN,
    );

    // Fillet the bottom of the top half of the BTU socket.
    cylinder_mask(
        l=h_btu_top + t_btu_rim,
        r=(d_btu_top + d_btu_top_clearance) / 2 + t_btu_socket,
        fillet1=1,
        orient=ORIENT_Z,
        align=V_DOWN,
        ends_only=true,
    );
  }

  translate([0, 0, -h_btu_top]) {
    // Inner part of rim joining top and bottom.
    tube(
      h=t_btu_rim,
      ir=(d_btu_bottom + d_btu_bottom_clearance) / 2,
      or=(d_btu_top + d_btu_top_clearance) / 2,
      orient=ORIENT_Z,
      align=V_DOWN,
    );

    // Bottom part of BTU socket.
    tube(
      h=h_btu_bottom + h_btu_bottom_clearance,
      ir=(d_btu_bottom + d_btu_bottom_clearance) / 2,
      wall=t_btu_socket,
      orient=ORIENT_Z,
      align=V_DOWN,
    );
  }
}
