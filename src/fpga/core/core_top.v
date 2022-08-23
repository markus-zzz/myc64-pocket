//
// User core top-level
//
// Instantiated by the real top-level: apf_top
//

`default_nettype none

module core_top (

//
// physical connections
//

///////////////////////////////////////////////////
// clock inputs 74.25mhz. not phase aligned, so treat these domains as asynchronous

input   wire            clk_74a, // mainclk1
input   wire            clk_74b, // mainclk1 

///////////////////////////////////////////////////
// cartridge interface
// switches between 3.3v and 5v mechanically
// output enable for multibit translators controlled by pic32

// GBA AD[15:8]
inout   wire    [7:0]   cart_tran_bank2,
output  wire            cart_tran_bank2_dir,

// GBA AD[7:0]
inout   wire    [7:0]   cart_tran_bank3,
output  wire            cart_tran_bank3_dir,

// GBA A[23:16]
inout   wire    [7:0]   cart_tran_bank1,
output  wire            cart_tran_bank1_dir,

// GBA [7] PHI#
// GBA [6] WR#
// GBA [5] RD#
// GBA [4] CS1#/CS#
//     [3:0] unwired
inout   wire    [7:4]   cart_tran_bank0,
output  wire            cart_tran_bank0_dir,

// GBA CS2#/RES#
inout   wire            cart_tran_pin30,
output  wire            cart_tran_pin30_dir,
// when GBC cart is inserted, this signal when low or weak will pull GBC /RES low with a special circuit
// the goal is that when unconfigured, the FPGA weak pullups won't interfere.
// thus, if GBC cart is inserted, FPGA must drive this high in order to let the level translators
// and general IO drive this pin.
output  wire            cart_pin30_pwroff_reset,

// GBA IRQ/DRQ
inout   wire            cart_tran_pin31,
output  wire            cart_tran_pin31_dir,

// infrared
input   wire            port_ir_rx,
output  wire            port_ir_tx,
output  wire            port_ir_rx_disable, 

// GBA link port
inout   wire            port_tran_si,
output  wire            port_tran_si_dir,
inout   wire            port_tran_so,
output  wire            port_tran_so_dir,
inout   wire            port_tran_sck,
output  wire            port_tran_sck_dir,
inout   wire            port_tran_sd,
output  wire            port_tran_sd_dir,
 
///////////////////////////////////////////////////
// cellular psram 0 and 1, two chips (64mbit x2 dual die per chip)

output  wire    [21:16] cram0_a,
inout   wire    [15:0]  cram0_dq,
input   wire            cram0_wait,
output  wire            cram0_clk,
output  wire            cram0_adv_n,
output  wire            cram0_cre,
output  wire            cram0_ce0_n,
output  wire            cram0_ce1_n,
output  wire            cram0_oe_n,
output  wire            cram0_we_n,
output  wire            cram0_ub_n,
output  wire            cram0_lb_n,

output  wire    [21:16] cram1_a,
inout   wire    [15:0]  cram1_dq,
input   wire            cram1_wait,
output  wire            cram1_clk,
output  wire            cram1_adv_n,
output  wire            cram1_cre,
output  wire            cram1_ce0_n,
output  wire            cram1_ce1_n,
output  wire            cram1_oe_n,
output  wire            cram1_we_n,
output  wire            cram1_ub_n,
output  wire            cram1_lb_n,

///////////////////////////////////////////////////
// sdram, 512mbit 16bit

output  wire    [12:0]  dram_a,
output  wire    [1:0]   dram_ba,
inout   wire    [15:0]  dram_dq,
output  wire    [1:0]   dram_dqm,
output  wire            dram_clk,
output  wire            dram_cke,
output  wire            dram_ras_n,
output  wire            dram_cas_n,
output  wire            dram_we_n,

///////////////////////////////////////////////////
// sram, 1mbit 16bit

output  wire    [16:0]  sram_a,
inout   wire    [15:0]  sram_dq,
output  wire            sram_oe_n,
output  wire            sram_we_n,
output  wire            sram_ub_n,
output  wire            sram_lb_n,

///////////////////////////////////////////////////
// vblank driven by dock for sync in a certain mode

input   wire            vblank,

///////////////////////////////////////////////////
// i/o to 6515D breakout usb uart

output  wire            dbg_tx,
input   wire            dbg_rx,

///////////////////////////////////////////////////
// i/o pads near jtag connector user can solder to

output  wire            user1,
input   wire            user2,

///////////////////////////////////////////////////
// RFU internal i2c bus 

inout   wire            aux_sda,
output  wire            aux_scl,

///////////////////////////////////////////////////
// RFU, do not use
output  wire            vpll_feed,


//
// logical connections
//

///////////////////////////////////////////////////
// video, audio output to scaler
output  wire    [23:0]  video_rgb,
output  wire            video_rgb_clock,
output  wire            video_rgb_clock_90,
output  wire            video_de,
output  wire            video_skip,
output  wire            video_vs,
output  wire            video_hs,
    
output  wire            audio_mclk,
input   wire            audio_adc,
output  wire            audio_dac,
output  wire            audio_lrck,

///////////////////////////////////////////////////
// bridge bus connection
// synchronous to clk_74a
output  wire            bridge_endian_little,
input   wire    [31:0]  bridge_addr,
input   wire            bridge_rd,
output  reg     [31:0]  bridge_rd_data,
input   wire            bridge_wr,
input   wire    [31:0]  bridge_wr_data,

///////////////////////////////////////////////////
// controller data
// 
// key bitmap:
//   [0]    dpad_up
//   [1]    dpad_down
//   [2]    dpad_left
//   [3]    dpad_right
//   [4]    face_a
//   [5]    face_b
//   [6]    face_x
//   [7]    face_y
//   [8]    trig_l1
//   [9]    trig_r1
//   [10]   trig_l2
//   [11]   trig_r2
//   [12]   trig_l3
//   [13]   trig_r3
//   [14]   face_select
//   [15]   face_start
// joy values - unsigned
//   [ 7: 0] lstick_x
//   [15: 8] lstick_y
//   [23:16] rstick_x
//   [31:24] rstick_y
// trigger values - unsigned
//   [ 7: 0] ltrig
//   [15: 8] rtrig
//
input   wire    [15:0]  cont1_key,
input   wire    [15:0]  cont2_key,
input   wire    [15:0]  cont3_key,
input   wire    [15:0]  cont4_key,
input   wire    [31:0]  cont1_joy,
input   wire    [31:0]  cont2_joy,
input   wire    [31:0]  cont3_joy,
input   wire    [31:0]  cont4_joy,
input   wire    [15:0]  cont1_trig,
input   wire    [15:0]  cont2_trig,
input   wire    [15:0]  cont3_trig,
input   wire    [15:0]  cont4_trig
    
);

// not using the IR port, so turn off both the LED, and
// disable the receive circuit to save power
assign port_ir_tx = 0;
assign port_ir_rx_disable = 1;

// bridge endianness
assign bridge_endian_little = 0;

// cart is unused, so set all level translators accordingly
// directions are 0:IN, 1:OUT
assign cart_tran_bank3 = 8'hzz;
assign cart_tran_bank3_dir = 1'b0;
assign cart_tran_bank2 = 8'hzz;
assign cart_tran_bank2_dir = 1'b0;
assign cart_tran_bank1 = 8'hzz;
assign cart_tran_bank1_dir = 1'b0;
assign cart_tran_bank0 = 4'hf;
assign cart_tran_bank0_dir = 1'b1;
assign cart_tran_pin30 = 1'b0;      // reset or cs2, we let the hw control it by itself
assign cart_tran_pin30_dir = 1'bz;
assign cart_pin30_pwroff_reset = 1'b0;  // hardware can control this
assign cart_tran_pin31 = 1'bz;      // input
assign cart_tran_pin31_dir = 1'b0;  // input

// link port is input only
assign port_tran_so = 1'bz;
assign port_tran_so_dir = 1'b0;     // SO is output only
assign port_tran_si = 1'bz;
assign port_tran_si_dir = 1'b0;     // SI is input only
assign port_tran_sck = 1'bz;
assign port_tran_sck_dir = 1'b0;    // clock direction can change
assign port_tran_sd = 1'bz;
assign port_tran_sd_dir = 1'b0;     // SD is input and not used

// tie off the rest of the pins we are not using
assign cram0_a = 'h0;
assign cram0_dq = {16{1'bZ}};
assign cram0_clk = 0;
assign cram0_adv_n = 1;
assign cram0_cre = 0;
assign cram0_ce0_n = 1;
assign cram0_ce1_n = 1;
assign cram0_oe_n = 1;
assign cram0_we_n = 1;
assign cram0_ub_n = 1;
assign cram0_lb_n = 1;

assign cram1_a = 'h0;
assign cram1_dq = {16{1'bZ}};
assign cram1_clk = 0;
assign cram1_adv_n = 1;
assign cram1_cre = 0;
assign cram1_ce0_n = 1;
assign cram1_ce1_n = 1;
assign cram1_oe_n = 1;
assign cram1_we_n = 1;
assign cram1_ub_n = 1;
assign cram1_lb_n = 1;

assign dram_a = 'h0;
assign dram_ba = 'h0;
assign dram_dq = {16{1'bZ}};
assign dram_dqm = 'h0;
assign dram_clk = 'h0;
assign dram_cke = 'h0;
assign dram_ras_n = 'h1;
assign dram_cas_n = 'h1;
assign dram_we_n = 'h1;

assign sram_a = 'h0;
assign sram_dq = {16{1'bZ}};
assign sram_oe_n  = 1;
assign sram_we_n  = 1;
assign sram_ub_n  = 1;
assign sram_lb_n  = 1;

assign dbg_tx = 1'bZ;
assign user1 = 1'bZ;
assign aux_scl = 1'bZ;
assign vpll_feed = 1'bZ;


// synchronizers for data that is launched in a different clock domain.
// all data read by bridge must be in the clk_74a (BRIDGE) domain.
	wire	[9:0]	square_x_s;
	wire	[9:0]	square_y_s;
	wire	[15:0]	frame_count_s;
synch_3 #(.WIDTH(10)) s30(square_x, square_x_s, clk_74a);
synch_3 #(.WIDTH(10)) s31(square_y, square_y_s, clk_74a);
synch_3 #(.WIDTH(16)) s32(frame_count, frame_count_s, clk_74a);


// for bridge write data, we just broadcast it to all bus devices
// for bridge read data, we have to mux it
// add your own devices here
always @(*) begin
	// note: all bridge read data needs to be synchronized into BRIDGE's clk_74 domain.
	// you should definitely synchronize going both ways in your application
	//
	// because this is a combinatorial block, you should have default cases
	// or otherwise complete coverage to prevent Quartus from inferring latches
    casex(bridge_addr)
	default: begin
		bridge_rd_data <= 0;
	end
	32'h002000xx: begin
		casex(bridge_addr[7:0])
		default: bridge_rd_data <= square_x_s;
		8'h04: bridge_rd_data <= square_y_s;
		endcase
	end
	32'h00F0000C: begin
		bridge_rd_data <= video_channel_enable;
	end
	32'h00100000: begin
		bridge_rd_data <= video_anim_enable;
	end
	32'h00100004: begin
		bridge_rd_data <= debug_value;
	end
	32'h00300000: begin
		bridge_rd_data <= signed_value;
	end
	32'h20000000: begin
		bridge_rd_data <= frame_count_s;
	end
	// for core_bridge_cmd
    32'hF8xxxxxx: begin
        bridge_rd_data <= cmd_bridge_rd_data;
    end
    endcase
end


//
// host/target command handler
//
    wire            reset_n;                // driven by host commands, can be used as core-wide reset
    wire    [31:0]  cmd_bridge_rd_data;
    
// bridge host commands
// synchronous to clk_74a
    wire            status_boot_done = pll_core_locked; 
    wire            status_setup_done = pll_core_locked; // rising edge triggers a target command
    wire            status_running = reset_n; // we are running as soon as reset_n goes high

    wire            dataslot_requestread;
    wire    [15:0]  dataslot_requestread_id;
    wire            dataslot_requestread_ack = 1;
    wire            dataslot_requestread_ok = 1;

    wire            dataslot_requestwrite;
    wire    [15:0]  dataslot_requestwrite_id;
    wire            dataslot_requestwrite_ack = 1;
    wire            dataslot_requestwrite_ok = 1;

    wire            dataslot_allcomplete;

    wire            savestate_supported;
    wire    [31:0]  savestate_addr;
    wire    [31:0]  savestate_size;
    wire    [31:0]  savestate_maxloadsize;

    wire            savestate_start;
    wire            savestate_start_ack;
    wire            savestate_start_busy;
    wire            savestate_start_ok;
    wire            savestate_start_err;

    wire            savestate_load;
    wire            savestate_load_ack;
    wire            savestate_load_busy;
    wire            savestate_load_ok;
    wire            savestate_load_err;
	
	wire            osnotify_inmenu;

// bridge target commands
// synchronous to clk_74a


// bridge data slot access

    wire    [9:0]   datatable_addr;
    wire            datatable_wren;
    wire    [31:0]  datatable_data;
    wire    [31:0]  datatable_q;

core_bridge_cmd icb (

    .clk                ( clk_74a ),
    .reset_n            ( reset_n ),

    .bridge_endian_little   ( bridge_endian_little ),
    .bridge_addr            ( bridge_addr ),
    .bridge_rd              ( bridge_rd ),
    .bridge_rd_data         ( cmd_bridge_rd_data ),
    .bridge_wr              ( bridge_wr ),
    .bridge_wr_data         ( bridge_wr_data ),
    
    .status_boot_done       ( status_boot_done ),
    .status_setup_done      ( status_setup_done ),
    .status_running         ( status_running ),

    .dataslot_requestread       ( dataslot_requestread ),
    .dataslot_requestread_id    ( dataslot_requestread_id ),
    .dataslot_requestread_ack   ( dataslot_requestread_ack ),
    .dataslot_requestread_ok    ( dataslot_requestread_ok ),

    .dataslot_requestwrite      ( dataslot_requestwrite ),
    .dataslot_requestwrite_id   ( dataslot_requestwrite_id ),
    .dataslot_requestwrite_ack  ( dataslot_requestwrite_ack ),
    .dataslot_requestwrite_ok   ( dataslot_requestwrite_ok ),

    .dataslot_allcomplete   ( dataslot_allcomplete ),

    .savestate_supported    ( savestate_supported ),
    .savestate_addr         ( savestate_addr ),
    .savestate_size         ( savestate_size ),
    .savestate_maxloadsize  ( savestate_maxloadsize ),

    .savestate_start        ( savestate_start ),
    .savestate_start_ack    ( savestate_start_ack ),
    .savestate_start_busy   ( savestate_start_busy ),
    .savestate_start_ok     ( savestate_start_ok ),
    .savestate_start_err    ( savestate_start_err ),

    .savestate_load         ( savestate_load ),
    .savestate_load_ack     ( savestate_load_ack ),
    .savestate_load_busy    ( savestate_load_busy ),
    .savestate_load_ok      ( savestate_load_ok ),
    .savestate_load_err     ( savestate_load_err ),

	.osnotify_inmenu        ( osnotify_inmenu ),

    .datatable_addr         ( datatable_addr ),
    .datatable_wren         ( datatable_wren ),
    .datatable_data         ( datatable_data ),
    .datatable_q            ( datatable_q ),

);


	reg	[2:0]	video_channel_enable = 3'b111;
	reg			video_anim_enable = 1;
	reg			video_resetsquare;
	reg			video_resetframe;
	reg			video_incrframe;
	
	reg			video_squareposx;
	reg			video_squareposy;
	reg	[9:0]	video_square_newx;
	reg	[9:0]	video_square_newy;
	
	reg	[31:0]	signed_value;
	
	reg	[31:0]	debug_value = 32'h12005678;

always @(posedge clk_74a) begin

	if(bridge_wr) begin
	  casex(bridge_addr)
		32'h002000xx: begin
			
			casex(bridge_addr[7:0])
			8'h00: begin
				video_square_newx <= bridge_wr_data;
				video_squareposx <= ~video_squareposx;
			end
			8'h04: begin
				video_square_newy <= bridge_wr_data;
				video_squareposy <= ~video_squareposy;
			end
			endcase
		end
		32'h00F0000C: begin
			video_channel_enable <= bridge_wr_data[2:0];
		end
		32'h00100000: begin
			video_anim_enable <= bridge_wr_data[0];
		end
		32'h00100004: begin
			debug_value <= bridge_wr_data;
		end
		32'h00300000: begin
			signed_value <= bridge_wr_data;
		end
		32'h00F00010: begin
			// toggle. the other clock domain will synchronize this and detect an edge
			video_resetsquare <= ~video_resetsquare;
		end
		32'h00F00014: begin
			// toggle. the other clock domain will synchronize this and detect an edge
			video_resetframe <= ~video_resetframe;
		end
		32'h00F00018: begin
			// toggle. the other clock domain will synchronize this and detect an edge
			video_incrframe <= ~video_incrframe;
		end
		endcase
	
	
	end

end

////////////////////////////////////////////////////////////////////////////////////////
//
// sychronizers for getting stuff from clk_74 (BRIDGE and others) into the video pixel
// clock domain (clk_core_12288/video_rgb_clock)
//
// this is very necessary and should not be neglected!
//
	wire	[2:0]	video_channel_enable_s;
	wire			video_anim_enable_s;
	wire			video_resetsquare_s;
	reg				video_resetsquare_last;
	wire			video_resetframe_s;
	reg				video_resetframe_last;
	wire			video_incrframe_s;
	reg				video_incrframe_last;
	wire			video_squareposx_s;
	reg				video_squareposx_last;
	wire			video_squareposy_s;
	reg				video_squareposy_last;
	wire	[9:0]	video_square_newx_s;
	wire	[9:0]	video_square_newy_s;
	reg				video_squareposx_nextcycle;
	reg				video_squareposy_nextcycle;
	
synch_3 #(.WIDTH(3)) s1(video_channel_enable, video_channel_enable_s, video_rgb_clock);
synch_3 			 s2(video_anim_enable, video_anim_enable_s, video_rgb_clock);
synch_3 			 s3(video_resetsquare, video_resetsquare_s, video_rgb_clock);
synch_3 			 s4(video_resetframe, video_resetframe_s, video_rgb_clock);
synch_3 			 s5(video_incrframe, video_incrframe_s, video_rgb_clock);

synch_3 			 s6(video_squareposx, video_squareposx_s, video_rgb_clock);
synch_3 			 s7(video_squareposy, video_squareposy_s, video_rgb_clock);
synch_3 #(.WIDTH(10)) s8(video_square_newx, video_square_newx_s, video_rgb_clock);
synch_3 #(.WIDTH(10)) s9(video_square_newy, video_square_newy_s, video_rgb_clock);



// video generation
// ~12,288,000 hz pixel clock
//
// we want our video mode of 320x240 @ 60hz, this results in 204800 clocks per frame
// we need to add hblank and vblank times to this, so there will be a nondisplay area. 
// it can be thought of as a border around the visible area.
// to make numbers simple, we can have 400 total clocks per line, and 320 visible.
// dividing 204800 by 400 results in 512 total lines per frame, and 240 visible.
// this pixel clock is fairly high for the relatively low resolution, but that's fine.
// PLL output has a minimum output frequency anyway.


assign video_rgb_clock = clk_core_12288;
assign video_rgb_clock_90 = clk_core_12288_90deg;
assign video_rgb = vidout_rgb;
assign video_de = vidout_de;
assign video_skip = vidout_skip;
assign video_vs = vidout_vs;
assign video_hs = vidout_hs;


// horizontal back porch: 10
// horizontal active: 320
// horizontal front porch: 70

// vertical back porch: 10
// vertical active: 240
// vertical front porch: 262

	localparam	VID_V_BPORCH = 'd10;
	localparam	VID_V_ACTIVE = 'd288;
	localparam	VID_V_TOTAL = 'd512;
	localparam	VID_H_BPORCH = 'd10;
	localparam	VID_H_ACTIVE = 'd320;
	localparam	VID_H_TOTAL = 'd400;
	
	reg	[15:0]	frame_count;
	
	reg	[9:0]	x_count;
	reg	[9:0]	y_count;
	
	wire [9:0]	visible_x = x_count - VID_H_BPORCH;
	wire [9:0]	visible_y = y_count - VID_V_BPORCH;

	reg	[23:0]	vidout_rgb;
	reg			vidout_de, vidout_de_1;
	reg			vidout_skip;
	reg			vidout_vs;
	reg			vidout_hs, vidout_hs_1;
	
	localparam	INIT_X = VID_H_ACTIVE / 2 - (50) / 2;
	localparam	INIT_Y = VID_V_ACTIVE / 2 - (50) / 2;
	
	reg	[9:0]	square_x = INIT_X;
	reg	[9:0]	square_y = INIT_Y;

always @(posedge video_rgb_clock or negedge reset_n) begin

	if(~reset_n) begin
		x_count <= 0;
		y_count <= 0;
		
	end else begin
		
		vidout_de <= 0;
		vidout_skip <= 0;
		vidout_vs <= 0;
		vidout_hs <= 0;
		
		vidout_hs_1 <= vidout_hs;
		vidout_de_1 <= vidout_de;
		
		video_resetsquare_last <= video_resetsquare_s;
		video_resetframe_last <= video_resetframe_s;
		video_incrframe_last <= video_incrframe_s;
		video_squareposx_last <= video_squareposx_s;
		video_squareposy_last <= video_squareposy_s;
		
		// x and y counters
		x_count <= x_count + 1'b1;
		if(x_count == VID_H_TOTAL-1) begin
			x_count <= 0;
			
			y_count <= y_count + 1'b1;
			if(y_count == VID_V_TOTAL-1) begin
				y_count <= 0;
			end
		end
		
		// generate sync 
		if(x_count == 0 && y_count == 0) begin
			// sync signal in back porch
			// new frame
			vidout_vs <= 1;
			
			if(video_anim_enable_s) begin
				frame_count <= frame_count + 1'b1;
			end
		end
		
		// we want HS to occur a bit after VS, not on the same cycle
		if(x_count == 3) begin
			// sync signal in back porch
			// new line
			vidout_hs <= 1;
		end

		// inactive screen areas are black
		vidout_rgb <= 24'h0;
		// generate active video
		if(x_count >= VID_H_BPORCH && x_count < VID_H_ACTIVE+VID_H_BPORCH) begin

			if(y_count >= VID_V_BPORCH && y_count < VID_V_ACTIVE+VID_V_BPORCH) begin
				// data enable. this is the active region of the line
				vidout_de <= 1;
				
				// generate the sliding XOR background
				vidout_rgb[23:16] <= (visible_x + frame_count / 1) ^ (visible_y + frame_count/1);
				vidout_rgb[15:8]  <= (visible_x + frame_count / 2) ^ (visible_y - frame_count/2);
				vidout_rgb[7:0]	  <= (visible_x - frame_count / 1) ^ (visible_y + 128);
				
				// blank out background channels if they are masked
				if(~video_channel_enable_s[2]) vidout_rgb[23:16] <= 0;
				if(~video_channel_enable_s[1]) vidout_rgb[15:8] <= 0;
				if(~video_channel_enable_s[0]) vidout_rgb[7:0] <= 0;
				
				// add colored borders for debugging
				if(visible_x == 0) begin
					vidout_rgb <= 24'hFFFFFF;
				end else if(visible_x == VID_H_ACTIVE-1) begin
					vidout_rgb <= 24'h00FF00;
				end else if(visible_y == 0) begin
					vidout_rgb <= 24'hFF0000;
				end else if(visible_y == VID_V_ACTIVE-1) begin
					vidout_rgb <= 24'h0000FF;
				end
				
				// generate square
				if(visible_x >= square_x && visible_x < square_x+50) begin
					if(visible_y >= square_y && visible_y < square_y+50) begin
						vidout_rgb <= 24'h0; 
					end
				end
				if(visible_x >= square_x+1 && visible_x < square_x+50-1) begin
					if(visible_y >= square_y+1 && visible_y < square_y+50-1) begin
						// change color of the square based on button state.
						// note: because the button state could change in the middle of the frame,
						// tearing on the square color could occur, but this is normal.
						if(cont1_key[4])	
							vidout_rgb <= 24'hFF00FF; 
						else if(cont1_key[5])	
							vidout_rgb <= 24'h00FF00; 
						else 
							vidout_rgb <= 24'hFFFFFF; 
					end
				end
				
			end 
		end
		
		if(vidout_vs) begin
			// vertical sync, new frame pulse (actually occurred on the previous cycle)
			// this will actually cause tearing but only on the upperleft-most pixel
			
			if(cont1_key[0]) begin
				// d-pad up
				if(square_y > 0) square_y <= square_y - 'd1;
			end
			if(cont1_key[1]) begin
				// d-pad down
				if(square_y < VID_V_ACTIVE-50) square_y <= square_y + 'd1;
			end
			if(cont1_key[2]) begin
				// d-pad left
				if(square_x > 0) square_x <= square_x - 'd1;
			end
			if(cont1_key[3]) begin
				// d-pad right
				if(square_x < VID_H_ACTIVE-50) square_x <= square_x + 'd1;
			end
		end
		
		// detect any edge coming from the synchronized square reset signal
		if(video_resetsquare_last != video_resetsquare_s) begin
			square_x <= INIT_X;
			square_y <= INIT_Y;
		end
		
		// detect any edge coming from the synchronized frame reset signal
		if(video_resetframe_last != video_resetframe_s) begin
			frame_count <= 0;
		end
		
		// detect any edge coming from the synchronized frame reset signal
		if(video_incrframe_last != video_incrframe_s) begin
			frame_count <= frame_count + 1'b1;
		end
		
		// detect any edge coming from the synchronized frame reset signal
		// then generate a delay signal
		if(video_squareposx_last != video_squareposx_s) begin
			video_squareposx_nextcycle <= 1;
		end else begin
			video_squareposx_nextcycle <= 0;
		end
		if(video_squareposy_last != video_squareposy_s) begin
			video_squareposy_nextcycle <= 1;
		end else begin
			video_squareposy_nextcycle <= 0;
		end
		// load the new square coordinates, but 1 cycle delayed so the 10-bit wide data
		// has settled
		if(video_squareposx_nextcycle) begin
			square_x <= video_square_newx_s;
		end
		if(video_squareposy_nextcycle) begin
			square_y <= video_square_newy_s;
		end
	end
end




//
// audio i2s silence generator
// see other examples for actual audio generation
//

assign audio_mclk = audgen_mclk;
assign audio_dac = audgen_dac;
assign audio_lrck = audgen_lrck;

// generate MCLK = 12.288mhz with fractional accumulator
    reg         [21:0]  audgen_accum = 0;
    reg                 audgen_mclk;
    parameter   [20:0]  CYCLE_48KHZ = 21'd122880 * 2;
always @(posedge clk_74a) begin
    audgen_accum <= audgen_accum + CYCLE_48KHZ;
    if(audgen_accum >= 21'd742500) begin
        audgen_mclk <= ~audgen_mclk;
        audgen_accum <= audgen_accum - 21'd742500 + CYCLE_48KHZ;
    end
end

// generate SCLK = 3.072mhz by dividing MCLK by 4
    reg [1:0]   aud_mclk_divider;
    wire        audgen_sclk = aud_mclk_divider[1] /* synthesis keep*/;
    reg         audgen_lrck_1;
always @(posedge audgen_mclk) begin
    aud_mclk_divider <= aud_mclk_divider + 1'b1;
end

// shift out audio data as I2S 
// 32 total bits per channel, but only 16 active bits at the start and then 16 dummy bits
//
    reg     [4:0]   audgen_lrck_cnt;    
    reg             audgen_lrck;
    reg             audgen_dac;
always @(negedge audgen_sclk) begin
    audgen_dac <= 1'b0;
    // 48khz * 64
    audgen_lrck_cnt <= audgen_lrck_cnt + 1'b1;
    if(audgen_lrck_cnt == 31) begin
        // switch channels
        audgen_lrck <= ~audgen_lrck;
        
    end 
end


///////////////////////////////////////////////


    wire    clk_core_12288;
    wire    clk_core_12288_90deg;
    
    wire    pll_core_locked;
    
mf_pllbase mp1 (
    .refclk         ( clk_74a ),
    .rst            ( 0 ),
    
    .outclk_0       ( clk_core_12288 ),
    .outclk_1       ( clk_core_12288_90deg ),
    
    .locked         ( pll_core_locked )
);


    
endmodule
