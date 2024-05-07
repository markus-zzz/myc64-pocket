//
// bridge host/target command handler
// 2022 Analogue
//

// mapped to 0xF8xxxxxx on bridge
// the spec is loose enough to allow implementation with either
// block rams and a soft CPU, or simply hard logic with some case statements.
//
// the implementation spec is documented, and depending on your application you
// may want to completely replace this module. this is only one of many
// possible ways to accomplish the host/target command system and data table.
//
// this module should always be clocked by a direct clock input and never a PLL,
// because it should report PLL lock status
//

`default_nettype none

module core_bridge_cmd (

    input  wire clk,
    output reg  reset_n,

    input  wire        bridge_endian_little,
    input  wire [31:0] bridge_addr,
    input  wire        bridge_rd,
    output reg  [31:0] bridge_rd_data,
    input  wire        bridge_wr,
    input  wire [31:0] bridge_wr_data,

    // all these signals should be synchronous to clk
    // add synchronizers if these need to be used in other clock domains
    input wire status_boot_done,   // assert when PLLs lock and logic is ready
    input wire status_setup_done,  // assert when core is happy with what's been loaded into it
    input wire status_running,     // assert when pocket's taken core out of reset and is running

    output reg osnotify_inmenu,

    input  wire i_cpu_clk,
    input  wire i_cpu_req,
    output wire o_cpu_ack_pulse,

    input  wire [31:0] i_cpu_addr,
    input  wire [31:0] i_cpu_wdata,
    input  wire [ 3:0] i_cpu_wstrb,
    output reg  [31:0] o_cpu_rdata
);

  // handle endianness
  reg  [31:0] bridge_wr_data_in;
  reg  [31:0] bridge_rd_data_out;

  wire        endian_little_s;
  synch_3 s01 (
      bridge_endian_little,
      endian_little_s,
      clk
  );

  always @(*) begin
    bridge_rd_data <= endian_little_s ? {
        bridge_rd_data_out[7:0],
        bridge_rd_data_out[15:8],
        bridge_rd_data_out[23:16],
        bridge_rd_data_out[31:24]
    } : bridge_rd_data_out;

    bridge_wr_data_in <= endian_little_s ? {
        bridge_wr_data[7:0],
        bridge_wr_data[15:8],
        bridge_wr_data[23:16],
        bridge_wr_data[31:24]
    } : bridge_wr_data;
  end

  // host

  reg [31:0] host_0;
  reg [31:0] host_4 = 'h20;  // host cmd parameter data at 0x20
  reg [31:0] host_8 = 'h40;  // host cmd response data at 0x40

  reg [31:0] host_20;  // parameter data
  reg [31:0] host_24;
  reg [31:0] host_28;
  reg [31:0] host_2C;

  reg [31:0] host_40;  // response data
  reg [31:0] host_44;
  reg [31:0] host_48;
  reg [31:0] host_4C;

  reg        host_cmd_start;
  reg [15:0] host_cmd_startval;
  reg [15:0] host_cmd;
  reg [15:0] host_resultcode;

  localparam [3:0] ST_IDLE = 'd0;
  localparam [3:0] ST_PARSE = 'd1;
  localparam [3:0] ST_WORK = 'd2;
  localparam [3:0] ST_DONE_OK = 'd13;
  localparam [3:0] ST_DONE_CODE = 'd14;
  localparam [3:0] ST_DONE_ERR = 'd15;
  reg [ 3:0] hstate;

  // target

  reg [31:0] target_0;
  reg [31:0] target_4 = 'h20;
  reg [31:0] target_8 = 'h40;

  reg [31:0] target_20;  // parameter data
  reg [31:0] target_24;
  reg [31:0] target_28;
  reg [31:0] target_2C;

  reg [31:0] target_40;  // response data
  reg [31:0] target_44;
  reg [31:0] target_48;
  reg [31:0] target_4C;

  reg [7:0] updated_slots;

  localparam [3:0] TARG_ST_IDLE = 'd0;
  localparam [3:0] TARG_ST_READYTORUN = 'd1;
  localparam [3:0] TARG_ST_DISPMSG = 'd2;
  localparam [3:0] TARG_ST_SLOTREAD = 'd3;
  localparam [3:0] TARG_ST_SLOTRELOAD = 'd4;
  localparam [3:0] TARG_ST_SLOTWRITE = 'd5;
  localparam [3:0] TARG_ST_SLOTFLUSH = 'd6;
  localparam [3:0] TARG_ST_WAITRESULT = 'd15;
  localparam [3:0] TARG_ST_DEBUG0 = 'd10;
  localparam [3:0] TARG_ST_DEBUG1 = 'd11;
  reg [3:0] tstate;

  reg       status_setup_done_1;
  reg       status_setup_done_queue;


  initial begin
    reset_n <= 0;
    osnotify_inmenu <= 0;
    status_setup_done_queue <= 0;
    updated_slots <= 0;
  end

  // See article 'Crossing the abyss: asynchronous signals in a synchronous world'
  // We are going for the 'Figure 7' setup.

  wire cpu_req_pulse;
  wire cpu_req_s;

  synch_3 s_cpu_req (
      .i(i_cpu_req),
      .o(cpu_req_s),
      .rise(cpu_req_pulse),
      .clk(clk)
  );
  synch_3 s_cpu_ack (
      .i(cpu_req_s),
      .rise(o_cpu_ack_pulse),
      .clk(i_cpu_clk)
  );


  always @(posedge clk) begin

    // detect a rising edge on the input signal
    // and flag a queue that will be cleared later
    status_setup_done_1 <= status_setup_done;
    if (status_setup_done & ~status_setup_done_1) begin
      status_setup_done_queue <= 1;
    end

    if (bridge_wr) begin
      casex (bridge_addr)
        32'hF8xx00xx: begin
          case (bridge_addr[7:0])
            8'h0: begin
              host_0 <= bridge_wr_data_in;  // command/status
              // check for command
              if (bridge_wr_data_in[31:16] == 16'h434D) begin
                // host wants us to do a command
                host_cmd_startval <= bridge_wr_data_in[15:0];
                host_cmd_start <= 1;
              end
            end
            8'h20: host_20 <= bridge_wr_data_in;  // parameter data regs
            8'h24: host_24 <= bridge_wr_data_in;
            8'h28: host_28 <= bridge_wr_data_in;
            8'h2C: host_2C <= bridge_wr_data_in;
          endcase
        end
        32'hF8xx10xx: begin
          case (bridge_addr[7:0])
            8'h0:  target_0 <= bridge_wr_data_in;  // command/status
            8'h4:  target_4 <= bridge_wr_data_in;  // parameter data pointer
            8'h8:  target_8 <= bridge_wr_data_in;  // response data pointer
            8'h40: target_40 <= bridge_wr_data_in;  // response data regs
            8'h44: target_44 <= bridge_wr_data_in;
            8'h48: target_48 <= bridge_wr_data_in;
            8'h4C: target_4C <= bridge_wr_data_in;
          endcase
        end
      endcase
    end
    if (bridge_rd) begin
      casex (bridge_addr)
        32'hF8xx00xx: begin
          case (bridge_addr[7:0])
            8'h0:  bridge_rd_data_out <= host_0;  // command/status
            8'h4:  bridge_rd_data_out <= host_4;  // parameter data pointer
            8'h8:  bridge_rd_data_out <= host_8;  // response data pointer
            8'h40: bridge_rd_data_out <= host_40;  // response data regs
            8'h44: bridge_rd_data_out <= host_44;
            8'h48: bridge_rd_data_out <= host_48;
            8'h4C: bridge_rd_data_out <= host_4C;
          endcase
        end
        32'hF8xx10xx: begin
          case (bridge_addr[7:0])
            8'h0:  bridge_rd_data_out <= target_0;
            8'h4:  bridge_rd_data_out <= target_4;
            8'h8:  bridge_rd_data_out <= target_8;
            8'h20: bridge_rd_data_out <= target_20;  // parameter data regs
            8'h24: bridge_rd_data_out <= target_24;
            8'h28: bridge_rd_data_out <= target_28;
            8'h2C: bridge_rd_data_out <= target_2C;
          endcase
        end
      endcase
    end





    // host > target command executer
    case (hstate)
      ST_IDLE: begin
        // there is no queueing. pocket will always make sure any outstanding host
        // commands are finished before starting another
        if (host_cmd_start) begin
          host_cmd_start <= 0;
          // save the command in case it gets clobbered later
          host_cmd <= host_cmd_startval;
          hstate <= ST_PARSE;
        end

      end
      ST_PARSE: begin
        // overwrite command semaphore with busy flag
        host_0 <= {16'h4255, host_cmd};

        case (host_cmd)
          16'h0000: begin
            // Request Status
            host_resultcode <= 1;  // default: booting
            if (status_boot_done) begin
              host_resultcode <= 2;  // setup
              if (status_setup_done) begin
                host_resultcode <= 3;  // idle
              end else if (status_running) begin
                host_resultcode <= 4;  // running
              end
            end
            hstate <= ST_DONE_CODE;
          end
          16'h0010: begin
            // Reset Enter
            reset_n <= 0;
            hstate  <= ST_DONE_OK;
          end
          16'h0011: begin
            // Reset Exit
            reset_n <= 1;
            hstate  <= ST_DONE_OK;
          end
          16'h0080: begin
            // Data slot request read
            host_resultcode <= 1;  // Not allowed ever
            hstate <= ST_DONE_CODE;
          end
          16'h0082: begin
            // Data slot request write

            // XXX: For some reason this also happen for 'deferload' and
            // framework will fault if we say 'not allowed ever' so instead lie
            // and say 'ready to write' (the writes will be ignored anyway).
            host_resultcode <= 0;
            hstate <= ST_DONE_CODE;
          end
          16'h008A: begin
            // Data slot update
            // XXX: Set bit in register corresponding to slot idx. Bit clears when register is read.
            if (host_20[31:3] == 0) begin
              updated_slots[host_20[2:0]] <= 1'b1;
            end
            hstate <= ST_DONE_OK;
          end
          16'h008F: begin
            // Data slot access all complete
            hstate <= ST_DONE_OK;
          end
          16'h00A0: begin
            // Savestate: Start/Query
            host_40 <= 0;
            host_44 <= 0;
            host_48 <= 0;

            hstate  <= ST_DONE_OK;
          end
          16'h00A4: begin
            // Savestate: Load/Query
            host_40 <= 0;
            host_44 <= 0;
            host_48 <= 0;

            hstate  <= ST_DONE_OK;
          end
          16'h00B0: begin
            // OS Notify: Menu State
            osnotify_inmenu <= host_20[0];
            hstate <= ST_DONE_OK;
          end
          default: begin
            hstate <= ST_DONE_ERR;
          end
        endcase
      end
      ST_WORK: begin
        hstate <= ST_IDLE;
      end
      ST_DONE_OK: begin
        host_0 <= 32'h4F4B0000;  // result code 0
        hstate <= ST_IDLE;
      end
      ST_DONE_CODE: begin
        host_0 <= {16'h4F4B, host_resultcode};
        hstate <= ST_IDLE;
      end
      ST_DONE_ERR: begin
        host_0 <= 32'h4F4BFFFF;  // result code FFFF = unknown command
        hstate <= ST_IDLE;
      end
    endcase

    // target > host command executer

    case (tstate)
      TARG_ST_IDLE: begin
        if (status_setup_done_queue) begin
          status_setup_done_queue <= 0;
          tstate <= TARG_ST_READYTORUN;
        end

      end
      TARG_ST_READYTORUN: begin
        target_0 <= 32'h636D_0140;
        tstate   <= TARG_ST_WAITRESULT;
      end
      TARG_ST_WAITRESULT: begin
        if (target_0[31:16] == 16'h6F6B) begin
          // done
          tstate <= TARG_ST_IDLE;
        end
      end
    endcase

    if (cpu_req_pulse && i_cpu_wstrb == 4'b1111) begin
      case (i_cpu_addr[7:0])
        8'h0:  target_0 <= i_cpu_wdata;  // command/status
        8'h4:  target_4 <= i_cpu_wdata;  // parameter data pointer
        8'h8:  target_8 <= i_cpu_wdata;  // response data pointer
        8'h20: target_20 <= i_cpu_wdata;  // response data regs
        8'h24: target_24 <= i_cpu_wdata;
        8'h28: target_28 <= i_cpu_wdata;
        8'h2C: target_2C <= i_cpu_wdata;
      endcase
    end

    if (cpu_req_pulse) begin
      case (i_cpu_addr[7:0])
        8'h0:  o_cpu_rdata <= target_0;  // command/status
        8'h4:  o_cpu_rdata <= target_4;  // parameter data pointer
        8'h8:  o_cpu_rdata <= target_8;  // response data pointer
        8'h40: o_cpu_rdata <= target_40;  // response data regs
        8'h44: o_cpu_rdata <= target_44;
        8'h48: o_cpu_rdata <= target_48;
        8'h4C: o_cpu_rdata <= target_4C;
        8'h80: begin
          o_cpu_rdata <= updated_slots;
          updated_slots <= 0;
        end
      endcase
    end

  end

endmodule
