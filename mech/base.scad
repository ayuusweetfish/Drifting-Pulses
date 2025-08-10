module holder(r_ext, r_int, thickness, height) {
  difference() {
    cylinder(thickness + height, r_ext, r_ext);
    translate([0, 0, thickness])
      cylinder(height, r_int, r_int);
  }
}

base_top_z = 120;
base_top_r = 15;
holder1_z = 150;
holder2_z = 180;

translate([0, 0, holder2_z]) holder(8, 7, 1, 2);
translate([0, 0, holder1_z]) holder(8, 7, 1, 2);
translate([0, 0, holder1_z]) {
  translate([0, -8, 0]) cylinder(holder2_z - holder1_z + 3, 1, 1);
  translate([0, +8, 0]) cylinder(holder2_z - holder1_z + 3, 1, 1);
}
translate([0, 0, base_top_z]) {
  h = holder1_z - base_top_z + 3;
  skew_rate = (8 - base_top_r) / h;
  I = [
    [1, 0, 0, 0],
    [0, 1, 0, 0],
    [0, 0, 1, 0],
    [0, 0, 0, 1],
  ];
  skew_m = [
    [0, 0, skew_rate, 0],
    [0, 0, 0, 0],
    [0, 0, 0, 0],
    [0, 0, 0, 0],
  ];
  translate([-base_top_r, 0, 0]) multmatrix(I - skew_m) cylinder(h, 1, 1);
  translate([+base_top_r, 0, 0]) multmatrix(I + skew_m) cylinder(h, 1, 1);
}
