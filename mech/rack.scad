unit_x = 22;
unit_y = 22;
n = 4;

unit_pitch = 100;
wire_slot = 8;
side_block_ext = 5;
side_slot_ext = 50;

shell_thickness = 1;
shell_front_height = 36;
shell_front_drop = 6;
shell_back_height = 12;
block_height = 4;

punch_r = 3;
punch_y = 4;

full_x = (side_slot_ext + side_block_ext) * 2 + unit_pitch * (n - 1) + unit_x;
full_y = unit_y;

module shell() union() {
  t = shell_thickness;
  // Bottom
  translate([-t, -t, -t])
    cube([full_x + t * 2, full_y + t * 2, t]);
  // Left, right
  translate([-t, -t, 0])
    cube([t, full_y + t * 2, shell_back_height]);
  translate([full_x, -t, 0])
    cube([t, full_y + t * 2, shell_back_height]);
  // Back
  translate([-t, full_y, 0])
    cube([full_x + t * 2, t, shell_back_height]);
  // Front
  translate([-t, -t, -shell_front_drop])
    cube([full_x + t * 2, t, shell_front_height]);
}

unit0_x = side_slot_ext + side_block_ext;

module shell_punched() difference() {
  shell();
  for (i = [0:n - 1]) {
    translate([unit0_x + unit_x / 2 + unit_pitch * i, 0, punch_y])
    rotate(90, [1, 0, 0])
      cylinder(shell_thickness, punch_r, punch_r);
  }
}

union() {
  shell_punched();
  for (i = [0:n - 2]) {
    translate([unit0_x + unit_x + unit_pitch * i, 0, 0])
      cube([unit_pitch - unit_x, full_y - wire_slot, block_height]);
  }
  translate([side_slot_ext, 0, 0])
    cube([side_block_ext, full_y - wire_slot, block_height]);
  translate([unit0_x + unit_pitch * (n - 1) + unit_x, 0, 0])
    cube([side_block_ext, full_y - wire_slot, block_height]);
}
