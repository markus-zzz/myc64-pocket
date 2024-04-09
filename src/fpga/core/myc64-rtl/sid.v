/*
 * Copyright (C) 2020-2024 Markus Lavin (https://www.zzzconsulting.se/)
 *
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

/* Implementation is mostly based on information from the official datasheet
 * and the interview with Bob Yannes
 *  - http://archive.6502.org/datasheets/mos_6582_sid.pdf
 *  - http://sid.kubarth.com/articles/interview_bob_yannes.html
 *
 * XXX: Currently many limitations such as (but not limited to)
 *  - No noise waveform.
 *  - No ring modulation.
 *  - No hard sync.
 *  - Linear envelope.
 *  - No filters.
 */

`default_nettype none

module sid(
  input clk,
  input rst,
  input clk_1mhz_ph1_en,
  input i_cs,
  input [4:0] i_addr,
  input i_we,
  input [7:0] i_data,
  output reg [7:0] o_data,
  output reg [15:0] o_wave
);
  wire [11:0] wave1, wave2, wave3;
  wire [7:0] env1, env2, env3;
  wire [20:0] voice1, voice2, voice3;

  always @(posedge clk) begin
    o_wave <= $signed({{2{voice1[20]}}, voice1[20:7]}) +
              $signed({{2{voice2[20]}}, voice2[20:7]}) +
              $signed({{2{voice3[20]}}, voice3[20:7]});
  end

  // Voice #1
  assign voice1 = $signed(wave1 - 12'h800) * $signed({1'b0, env1});
  waveform_gen u_wave_1(
    .clk(clk),
    .rst(rst),
    .clk_1mhz_ph1_en(clk_1mhz_ph1_en),
    .i_frequency({r_d401, r_d400}),
    .i_duty_cycle({r_d403, r_d402}),
    .i_triangle_en(r_d404[4]),
    .i_sawtooth_en(r_d404[5]),
    .i_pulse_en(r_d404[6]),
    .o_wave(wave1)
  );

  envelope_gen u_envelope_1(
    .clk(clk),
    .rst(rst),
    .clk_1mhz_ph1_en(clk_1mhz_ph1_en),
    .i_gate(r_d404[0]),
    .i_attack(r_d405[7:4]),
    .i_decay(r_d405[3:0]),
    .i_sustain(r_d406[7:4]),
    .i_release(r_d406[3:0]),
    .o_envelope(env1)
  );

  // Voice #2
  assign voice2 = $signed(wave2 - 12'h800) * $signed({1'b0, env2});
  waveform_gen u_wave_2(
    .clk(clk),
    .rst(rst),
    .clk_1mhz_ph1_en(clk_1mhz_ph1_en),
    .i_frequency({r_d408, r_d407}),
    .i_duty_cycle({r_d40a, r_d409}),
    .i_triangle_en(r_d40b[4]),
    .i_sawtooth_en(r_d40b[5]),
    .i_pulse_en(r_d40b[6]),
    .o_wave(wave2)
  );

  envelope_gen u_envelope_2(
    .clk(clk),
    .rst(rst),
    .clk_1mhz_ph1_en(clk_1mhz_ph1_en),
    .i_gate(r_d40b[0]),
    .i_attack(r_d40c[7:4]),
    .i_decay(r_d40c[3:0]),
    .i_sustain(r_d40d[7:4]),
    .i_release(r_d40d[3:0]),
    .o_envelope(env2)
  );

  // Voice #3
  assign voice3 = $signed(wave3 - 12'h800) * $signed({1'b0, env3});
  waveform_gen u_wave_3(
    .clk(clk),
    .rst(rst),
    .clk_1mhz_ph1_en(clk_1mhz_ph1_en),
    .i_frequency({r_d40f, r_d40e}),
    .i_duty_cycle({r_d411, r_d410}),
    .i_triangle_en(r_d412[4]),
    .i_sawtooth_en(r_d412[5]),
    .i_pulse_en(r_d412[6]),
    .o_wave(wave3)
  );

  envelope_gen u_envelope_3(
    .clk(clk),
    .rst(rst),
    .clk_1mhz_ph1_en(clk_1mhz_ph1_en),
    .i_gate(r_d412[0]),
    .i_attack(r_d413[7:4]),
    .i_decay(r_d413[3:0]),
    .i_sustain(r_d414[7:4]),
    .i_release(r_d414[3:0]),
    .o_envelope(env3)
  );

  reg [7:0] r_d400; // Voice #1 freq lo-byte.
  reg [7:0] r_d401; // Voice #1 freq hi-byte.
  reg [7:0] r_d402; // Voice #1 pulse-width lo-byte.
  reg [3:0] r_d403; // Voice #1 pulse-width hi-byte.
  reg [7:0] r_d404; // Voice #1 ctrl register.
  reg [7:0] r_d405; // Voice #1 Attack and Decay length.
  reg [7:0] r_d406; // Voice #1 Sustain volume and Release length.

  reg [7:0] r_d407; // Voice #2 freq lo-byte.
  reg [7:0] r_d408; // Voice #2 freq hi-byte.
  reg [7:0] r_d409; // Voice #2 pulse-width lo-byte.
  reg [3:0] r_d40a; // Voice #2 pulse-width hi-byte.
  reg [7:0] r_d40b; // Voice #2 ctrl register.
  reg [7:0] r_d40c; // Voice #2 Attack and Decay length.
  reg [7:0] r_d40d; // Voice #2 Sustain volume and Release length.

  reg [7:0] r_d40e; // Voice #3 freq lo-byte.
  reg [7:0] r_d40f; // Voice #3 freq hi-byte.
  reg [7:0] r_d410; // Voice #3 pulse-width lo-byte.
  reg [3:0] r_d411; // Voice #3 pulse-width hi-byte.
  reg [7:0] r_d412; // Voice #3 ctrl register.
  reg [7:0] r_d413; // Voice #3 Attack and Decay length.
  reg [7:0] r_d414; // Voice #3 Sustain volume and Release length.

  always @(posedge clk) begin
    if (rst) begin
      r_d400 <= 0;
      r_d401 <= 0;
      r_d402 <= 0;
      r_d403 <= 0;
      r_d404 <= 0;
      r_d405 <= 0;
      r_d406 <= 0;

      r_d407 <= 0;
      r_d408 <= 0;
      r_d409 <= 0;
      r_d40a <= 0;
      r_d40b <= 0;
      r_d40c <= 0;
      r_d40d <= 0;

      r_d40e <= 0;
      r_d40f <= 0;
      r_d410 <= 0;
      r_d411 <= 0;
      r_d412 <= 0;
      r_d413 <= 0;
      r_d414 <= 0;
    end
    else if (clk_1mhz_ph1_en & i_cs & i_we) begin
      case (i_addr)
        5'h00: r_d400 <= i_data[7:0];
        5'h01: r_d401 <= i_data[7:0];
        5'h02: r_d402 <= i_data[7:0];
        5'h03: r_d403 <= i_data[3:0];
        5'h04: r_d404 <= i_data[7:0];
        5'h05: r_d405 <= i_data[7:0];
        5'h06: r_d406 <= i_data[7:0];

        5'h07: r_d407 <= i_data[7:0];
        5'h08: r_d408 <= i_data[7:0];
        5'h09: r_d409 <= i_data[7:0];
        5'h0a: r_d40a <= i_data[3:0];
        5'h0b: r_d40b <= i_data[7:0];
        5'h0c: r_d40c <= i_data[7:0];
        5'h0d: r_d40d <= i_data[7:0];

        5'h0e: r_d40e <= i_data[7:0];
        5'h0f: r_d40f <= i_data[7:0];
        5'h10: r_d410 <= i_data[7:0];
        5'h11: r_d411 <= i_data[3:0];
        5'h12: r_d412 <= i_data[7:0];
        5'h13: r_d413 <= i_data[7:0];
        5'h14: r_d414 <= i_data[7:0];
        default: /* do nothing */;
      endcase
    end
  end

  always @* begin
    case (i_addr)
      5'h1b: o_data = wave3[11:4];
      5'h1c: o_data = env3;
      default: o_data = 0;
    endcase
  end

endmodule

module waveform_gen(
  input clk,
  input rst,
  input clk_1mhz_ph1_en,
  input [15:0] i_frequency,
  input [11:0] i_duty_cycle,
  input i_triangle_en,
  input i_sawtooth_en,
  input i_pulse_en,
  output [11:0] o_wave
);
  reg [23:0] phase_accum;
  wire [11:0] wave_sawtooth, wave_triangle, wave_pulse;

  always @(posedge clk) begin
    if (rst)
      phase_accum <= 0;
    else if (clk_1mhz_ph1_en)
      phase_accum <= phase_accum + {8'h00, i_frequency};
  end

  assign wave_sawtooth = phase_accum[23:12];
  assign wave_triangle = {{11{phase_accum[23]}} ^ phase_accum[22:12], 1'b0};
  assign wave_pulse = phase_accum[23:12] <= i_duty_cycle ? 12'hfff : 12'h0;

  assign o_wave = ~(~({12{i_sawtooth_en}} & wave_sawtooth) &
                    ~({12{i_triangle_en}} & wave_triangle) &
                    ~({12{i_pulse_en}} & wave_pulse));
endmodule

module envelope_gen(
  input clk,
  input rst,
  input clk_1mhz_ph1_en,
  input i_gate,
  input [3:0] i_attack,
  input [3:0] i_decay,
  input [3:0] i_sustain,
  input [3:0] i_release,
  output [7:0] o_envelope
);
  function [16:0] map_attack;
    input [3:0] value;
    reg [16:0] res;
    case (value)
      4'h0: res = 17'h7;     // 0.002
      4'h1: res = 17'h1f;    // 0.008
      4'h2: res = 17'h3e;    // 0.016
      4'h3: res = 17'h5d;    // 0.024
      4'h4: res = 17'h94;    // 0.038
      4'h5: res = 17'hda;    // 0.056
      4'h6: res = 17'h109;   // 0.068
      4'h7: res = 17'h138;   // 0.08
      4'h8: res = 17'h186;   // 0.1
      4'h9: res = 17'h3d0;   // 0.25
      4'ha: res = 17'h7a1;   // 0.5
      4'hb: res = 17'hc35;   // 0.8
      4'hc: res = 17'hf42;   // 1.0
      4'hd: res = 17'h2dc6;  // 3.0
      4'he: res = 17'h4c4b;  // 5.0
      4'hf: res = 17'h7a12;  // 8.0
    endcase
    map_attack = res;
  endfunction

  function [16:0] map_decay_release;
    input [3:0] value;
    reg [16:0] res;
    case (value)
      4'h0: res = 17'h17;     //  0.006
      4'h1: res = 17'h5d;     //  0.024
      4'h2: res = 17'hbb;     //  0.048
      4'h3: res = 17'h119;    //  0.072
      4'h4: res = 17'h1bd;    //  0.114
      4'h5: res = 17'h290;    //  0.168
      4'h6: res = 17'h31c;    //  0.204
      4'h7: res = 17'h3a9;    //  0.24
      4'h8: res = 17'h493;    //  0.3
      4'h9: res = 17'hb71;    //  0.75
      4'ha: res = 17'h16e3;   //  1.5
      4'hb: res = 17'h249f;   //  2.4
      4'hc: res = 17'h2dc6;   //  3.0
      4'hd: res = 17'h8954;   //  9.0
      4'he: res = 17'he4e1;   // 15.0
      4'hf: res = 17'h16e36;  // 24.0
    endcase
    map_decay_release = res;
  endfunction


  parameter s_idle = 1,
            s_attack = 2,
            s_decay = 3,
            s_sustain = 4,
            s_release = 5;

  reg [2:0] state;
  reg [7:0] cntr;
  reg [16:0] freq_div;

  assign o_envelope = cntr;

  always @(posedge clk) begin
    if (rst) begin
      state <= s_idle;
      cntr <= 0;
      freq_div <= 0;
    end
    else if (clk_1mhz_ph1_en) begin
      freq_div <= freq_div - 17'h001;
      case (state)
        s_idle: begin
          if (i_gate) begin
            state <= s_attack;
          end
        end
        s_attack: begin
          if (~i_gate) begin
            state <= s_release;
          end
          else if (cntr == 8'hff) begin
            state <= s_decay;
          end
          else if (freq_div == 0) begin
            cntr <= cntr + 8'h01;
            freq_div <= map_attack(i_attack);
          end
        end
        s_decay: begin
          if (~i_gate) begin
            state <= s_release;
          end
          else if (cntr == {i_sustain, 4'h0}) begin
            state <= s_sustain;
          end
          else if (freq_div == 0) begin
            cntr <= cntr - 8'h01;
            freq_div <= map_decay_release(i_decay);
          end
        end
        s_sustain: begin
          if (~i_gate) begin
            state <= s_release;
          end
        end
        s_release: begin
          if (i_gate) begin
            state <= s_attack;
          end
          else if (cntr == 8'h00) begin
            state <= s_idle;
          end
          else if (freq_div == 0) begin
            cntr <= cntr - 8'h01;
            freq_div <= map_decay_release(i_release);
          end
        end
      endcase
    end
  end

endmodule
/*
module tb;

  reg clk, rst;
  reg gate;

  sid u_sid(
    .clk(clk),
    .rst(rst),
    .clk_1mhz_ph1_en(1'b1),
    .i_gate(gate)
  );

  initial begin
    $dumpfile("dump.vcd");
    $dumpvars;
    rst = 1;
    clk = 0;
    #4 rst = 0;
    gate = 1;
    #450000 gate = 0;
    #500000 $finish;
  end

  always clk = #1 ~clk;

endmodule
*/
