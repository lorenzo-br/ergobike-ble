// ESP32-C3 + closed 18650 enclosure
// V8: same as V7, plus matching wire-port hole on the ESP32 enclosure side
// so the battery cable can actually pass between the two compartments.

$fn = 72;

body_file = "/mnt/data/obj_1_Body.stl";

// Original ESP32 enclosure reference
body_xmax = 156.75;
body_y_center = 128.00;

// Battery base
plate_x0 = body_xmax - 2.0;
plate_w = 31.0;
plate_len = 90.0;
plate_y0 = body_y_center - plate_len/2;
plate_t = 3.0;
plate_r = 4.0;

// Closed battery enclosure
wall_t = 2.2;
wall_h = 21.0;
outer_x0 = body_xmax + 1.8;
outer_y0 = plate_y0 + 1.0;
outer_w = 27.2;
outer_len = plate_len - 2.0;
outer_r = 3.6;

inner_x0 = outer_x0 + wall_t;
inner_y0 = outer_y0 + wall_t;
inner_w = outer_w - 2*wall_t;
inner_len = outer_len - 2*wall_t;
inner_r = max(0.8, outer_r-wall_t);

holder_x_center = inner_x0 + inner_w/2;
outer_mount_spacing = 72.9;
terminal_spacing = 55.6;
mount_slot_len = 7.0;
mount_slot_w = 3.4;
terminal_clearance_d = 4.2;
center_hole_d = 3.4;

// Battery lid
lid_t = 2.6;
lid_lip_h = 3.8;
lid_lip_t = 1.25;
lid_clear = 0.40;
lid_overhang = 0.5;

// Lid screws on the ends
boss_d = 7.0;
boss_r = boss_d/2;
boss_x = holder_x_center;
boss_y1 = outer_y0 - boss_r + 1.2;
boss_y2 = outer_y0 + outer_len + boss_r - 1.2;
boss_top_z = plate_t + wall_h;
boss_pilot_d = 2.25;
lid_screw_clear_d = 2.9;
lid_head_d = 5.4;
lid_head_depth = 1.4;

// Wire port between compartments.
wire_port_len = 6.0;
wire_port_d = 3.2;
wire_port_z = plate_t + 5.5;
// Cut distance through the original ESP body side wall.
esp_wire_cut_len = 4.2;
// Cut distance through the battery-box ESP-facing wall.
bat_wire_cut_len = wall_t + 1.0;

// Zip-tie mounts for 20 mm tube.
tie_y1 = body_y_center - 24.0;
tie_y2 = body_y_center + 24.0;
tie_mount_w_y = 8.6;
tie_web_t_y = 1.35;
tie_gap_x = 2.25;
outer_bar_t_x = 5.2;
tie_z0 = 4.0;
tie_h = 15.5;
wall_outer_x = outer_x0 + outer_w;
bar_x0 = wall_outer_x + tie_gap_x;

tube_d = 20.0;
saddle_d = 20.8;
tube_center_x = 201.4;
tube_center_z = tie_z0 + tie_h/2;
tube_ref_len = outer_len + 10.0;

module rounded_rect_2d(w,l,r){
    hull(){
        for (x=[r,w-r])
            for (y=[r,l-r])
                translate([x,y]) circle(r=r);
    }
}

module rounded_rect_prism(x0,y0,w,l,r,h){
    translate([x0,y0,0])
        linear_extrude(height=h)
            rounded_rect_2d(w,l,r);
}

module y_slot(xc,yc,total_len,width,h){
    rr = width/2;
    sep = max(0.01,total_len-width);
    hull(){
        translate([xc,yc-sep/2,-0.5]) cylinder(r=rr,h=h+0.5);
        translate([xc,yc+sep/2,-0.5]) cylinder(r=rr,h=h+0.5);
    }
}

module support_rib(yc){
    hull(){
        translate([body_xmax-1.0,yc-2.5,2.2]) cube([1.6,5.0,5.8]);
        translate([body_xmax+2.0,yc-2.5,2.2]) cube([1.0,5.0,1.2]);
    }
}

module wire_port_cut(x_start, x_len){
    hull(){
        for (yy=[body_y_center-(wire_port_len-wire_port_d)/2,
                 body_y_center+(wire_port_len-wire_port_d)/2])
            translate([x_start,yy,wire_port_z])
                rotate([0,90,0])
                    cylinder(d=wire_port_d,h=x_len);
    }
}

module battery_box_walls(){
    difference(){
        translate([outer_x0,outer_y0,plate_t])
            linear_extrude(height=wall_h)
                rounded_rect_2d(outer_w,outer_len,outer_r);

        translate([inner_x0,inner_y0,plate_t-0.1])
            linear_extrude(height=wall_h+0.3)
                rounded_rect_2d(inner_w,inner_len,inner_r);

        // Wire port through the ESP-facing wall of the battery box.
        wire_port_cut(outer_x0-0.5, bat_wire_cut_len);
    }
}

module external_end_lid_boss(yc){
    difference(){
        translate([boss_x,yc,0]) cylinder(d=boss_d,h=boss_top_z);
        translate([boss_x,yc,2.0]) cylinder(d=boss_pilot_d,h=boss_top_z-1.5);
    }
}

module end_boss_floor_bridge(yc, direction){
    hull(){
        translate([boss_x,yc,0]) cylinder(d=boss_d,h=plate_t);
        translate([boss_x,yc + direction*3.0,0]) cylinder(d=4.0,h=plate_t);
    }
}

module zip_tie_mount(yc){
    difference(){
        union(){
            translate([bar_x0, yc-tie_mount_w_y/2, tie_z0])
                cube([outer_bar_t_x,tie_mount_w_y,tie_h]);

            translate([wall_outer_x-0.3, yc-tie_mount_w_y/2, tie_z0])
                cube([tie_gap_x+outer_bar_t_x+0.3,tie_web_t_y,tie_h]);
            translate([wall_outer_x-0.3, yc+tie_mount_w_y/2-tie_web_t_y, tie_z0])
                cube([tie_gap_x+outer_bar_t_x+0.3,tie_web_t_y,tie_h]);
        }

        translate([tube_center_x, yc-tie_mount_w_y/2-0.5, tube_center_z])
            rotate([-90,0,0]) cylinder(d=saddle_d,h=tie_mount_w_y+1.0);
    }
}

module integrated_body(){
    difference(){
        union(){
            import(body_file,convexity=10);
            rounded_rect_prism(plate_x0,plate_y0,plate_w,plate_len,plate_r,plate_t);
            support_rib(body_y_center-16.0);
            support_rib(body_y_center+16.0);
            battery_box_walls();

            end_boss_floor_bridge(boss_y1,+1);
            end_boss_floor_bridge(boss_y2,-1);
            external_end_lid_boss(boss_y1);
            external_end_lid_boss(boss_y2);

            zip_tie_mount(tie_y1);
            zip_tie_mount(tie_y2);
        }

        // Holder fixing slots: floor only.
        y_slot(holder_x_center,body_y_center-outer_mount_spacing/2,
               mount_slot_len,mount_slot_w,plate_t+0.35);
        y_slot(holder_x_center,body_y_center+outer_mount_spacing/2,
               mount_slot_len,mount_slot_w,plate_t+0.35);

        // Terminal clearance: floor only.
        translate([holder_x_center,body_y_center-terminal_spacing/2,-0.5])
            cylinder(d=terminal_clearance_d,h=plate_t+0.85);
        translate([holder_x_center,body_y_center+terminal_spacing/2,-0.5])
            cylinder(d=terminal_clearance_d,h=plate_t+0.85);
        translate([holder_x_center,body_y_center,-0.5])
            cylinder(d=center_hole_d,h=plate_t+0.85);

        // NEW: matching wire port in the ESP32 enclosure side wall.
        // This aligns with the battery-box hole so the cable really passes through.
        wire_port_cut(body_xmax-esp_wire_cut_len+0.4, esp_wire_cut_len);
    }
}

module lid_end_ear(yc, direction){
    hull(){
        translate([boss_x,yc,boss_top_z]) cylinder(d=boss_d,h=lid_t);
        translate([boss_x,yc + direction*3.0,boss_top_z]) cylinder(d=4.0,h=lid_t);
    }
}

module battery_lid(){
    difference(){
        union(){
            translate([outer_x0-lid_overhang,outer_y0-lid_overhang,boss_top_z])
                linear_extrude(height=lid_t)
                    rounded_rect_2d(outer_w+2*lid_overhang,
                                    outer_len+2*lid_overhang,
                                    outer_r+lid_overhang);

            translate([inner_x0+lid_clear,inner_y0+lid_clear,boss_top_z-lid_lip_h+0.2])
                difference(){
                    linear_extrude(height=lid_lip_h)
                        rounded_rect_2d(inner_w-2*lid_clear,
                                        inner_len-2*lid_clear,
                                        max(0.8,inner_r-lid_clear));
                    translate([lid_lip_t,lid_lip_t,-0.1])
                        linear_extrude(height=lid_lip_h+0.2)
                            rounded_rect_2d(inner_w-2*lid_clear-2*lid_lip_t,
                                            inner_len-2*lid_clear-2*lid_lip_t,
                                            max(0.6,inner_r-lid_clear-lid_lip_t));
                }

            lid_end_ear(boss_y1,+1);
            lid_end_ear(boss_y2,-1);
        }

        translate([outer_x0-lid_overhang-0.2,body_y_center-8,boss_top_z-0.2])
            cube([4.0,16.0,lid_t+1.0]);

        for (yy=[boss_y1,boss_y2]) {
            translate([boss_x,yy,boss_top_z-0.2])
                cylinder(d=lid_screw_clear_d,h=lid_t+0.5);
            translate([boss_x,yy,boss_top_z+lid_t-lid_head_depth])
                cylinder(d=lid_head_d,h=lid_head_depth+0.25);
        }
    }
}

module tube_reference(){
    color([0.55,0.55,0.55,0.35])
    translate([tube_center_x, outer_y0-5.0, tube_center_z])
        rotate([-90,0,0]) cylinder(d=tube_d,h=tube_ref_len);
}



// ---------- V9 additions: screw-on ESP lid ----------
part = "assembly"; // [body,esp_lid,battery_lid,assembly,tube_reference]

esp_x0 = 128.25;
esp_x1 = 156.75;
esp_y0 = 107.75;
esp_y1 = 148.25;
esp_w = esp_x1-esp_x0;
esp_l = esp_y1-esp_y0;
esp_cx = (esp_x0+esp_x1)/2;
esp_cy = (esp_y0+esp_y1)/2;
esp_top_z = 21.45;

esp_lid_t = 2.6;
esp_lid_overhang = 0.45;
esp_lid_r = 2.8;

// Conservative shallow locating ring inside the original opening.
esp_lip_w = 25.0;
esp_lip_l = 37.0;
esp_lip_t = 1.15;
esp_lip_h = 2.0;
esp_lip_x0 = esp_cx - esp_lip_w/2;
esp_lip_y0 = esp_cy - esp_lip_l/2;
esp_lip_r = 1.5;

// Two lid screws on the clear side opposite the battery compartment.
esp_boss_d = 7.0;
esp_boss_x = 125.55;
esp_boss_y1 = esp_cy - 12.0;
esp_boss_y2 = esp_cy + 12.0;
esp_boss_pilot_d = 2.25;
esp_lid_clear_d = 2.9;
esp_lid_head_d = 5.4;
esp_lid_head_depth = 1.4;

module esp_lid_boss(yc){
    difference(){
        translate([esp_boss_x,yc,0]) cylinder(d=esp_boss_d,h=esp_top_z);
        translate([esp_boss_x,yc,2.0]) cylinder(d=esp_boss_pilot_d,h=esp_top_z-1.3);
    }
}

module esp_boss_floor_bridge(yc){
    hull(){
        translate([esp_boss_x,yc,0]) cylinder(d=esp_boss_d,h=3.0);
        translate([esp_x0+0.55,yc,0]) cylinder(d=3.8,h=3.0);
    }
}

module body_v9(){
    union(){
        integrated_body();
        esp_boss_floor_bridge(esp_boss_y1);
        esp_boss_floor_bridge(esp_boss_y2);
        esp_lid_boss(esp_boss_y1);
        esp_lid_boss(esp_boss_y2);
    }
}

module esp_lid_v9(){
    difference(){
        union(){
            translate([esp_x0-esp_lid_overhang,esp_y0-esp_lid_overhang,esp_top_z])
                linear_extrude(height=esp_lid_t)
                    rounded_rect_2d(esp_w+2*esp_lid_overhang,
                                    esp_l+2*esp_lid_overhang,
                                    esp_lid_r);

            // Hollow locating ring under the lid, no snap hooks.
            translate([esp_lip_x0,esp_lip_y0,esp_top_z-esp_lip_h+0.15])
                difference(){
                    linear_extrude(height=esp_lip_h)
                        rounded_rect_2d(esp_lip_w,esp_lip_l,esp_lip_r);
                    translate([esp_lip_t,esp_lip_t,-0.1])
                        linear_extrude(height=esp_lip_h+0.2)
                            rounded_rect_2d(esp_lip_w-2*esp_lip_t,
                                            esp_lip_l-2*esp_lip_t,
                                            max(0.6,esp_lip_r-esp_lip_t));
                }

            // Rounded ears reach the two external screw bosses.
            for (yy=[esp_boss_y1,esp_boss_y2])
                hull(){
                    translate([esp_boss_x,yy,esp_top_z])
                        cylinder(d=esp_boss_d,h=esp_lid_t);
                    translate([esp_x0-0.1,yy,esp_top_z])
                        cylinder(d=4.0,h=esp_lid_t);
                }
        }

        for (yy=[esp_boss_y1,esp_boss_y2]){
            translate([esp_boss_x,yy,esp_top_z-0.2])
                cylinder(d=esp_lid_clear_d,h=esp_lid_t+0.5);
            translate([esp_boss_x,yy,esp_top_z+esp_lid_t-esp_lid_head_depth])
                cylinder(d=esp_lid_head_d,h=esp_lid_head_depth+0.25);
        }
    }
}

if(part=="body") body_v9();
else if(part=="esp_lid") esp_lid_v9();
else if(part=="battery_lid") battery_lid();
else if(part=="tube_reference") tube_reference();
else {
    color([0.12,0.38,0.68,1]) body_v9();
    color([0.22,0.72,0.40,1]) esp_lid_v9();
    color([0.95,0.55,0.08,1]) battery_lid();
    tube_reference();
}
