$fn = 120;

eps = 0.1;

H = 24;
W_wall = 1.5;
R_base = 30;
R_top = 32;
R_mid = 35;

function r(z) =
  R_base
  + (R_mid - R_base) * sin(z/H * 180)
  + (R_top - R_base) * pow(z/H, 2);


rotate_extrude()
  polygon(concat(
    [for (z = [0 : 0.1 : H]) [r(z), z]],
    [for (z = [H : -0.1 : 0]) [r(z) - W_wall, z]]
  ));

H_support = 15;
H_board = 1.2;
R_board = 30.5;

translate([0, 0, H_support]) {
  difference() {
    union() {
      difference() {
        cylinder(h = H_board, r = r(H_support) - W_wall / 2, center = false);
        cylinder(h = H_board+eps, r = R_board, center = false);
        rotate([0, 0, 45]) cube([r(H_support), r(H_support), H_board+eps], center = false);
        rotate([0, 0, 225]) cube([r(H_support), r(H_support), H_board+eps], center = false);
      }
      translate([0, 0, -W_wall])
        cylinder(h = W_wall, r = r(H_support - W_wall) - W_wall / 2, center = false);
    }
    cube([50, 15, 10], center = true);
  }
}