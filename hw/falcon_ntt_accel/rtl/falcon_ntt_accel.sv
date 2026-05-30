module falcon_ntt_accel (
    input  logic clk_i,
    input  logic rst_ni,

    input  reg_pkg::reg_req_t reg_req_i,
    output reg_pkg::reg_rsp_t reg_rsp_o
);

  localparam int unsigned NUM_WORDS = 4;
  localparam int unsigned LATENCY_CYCLES = 8;

  localparam logic [31:0] Q   = 32'd12289;
  localparam logic [31:0] Q0I = 32'd12287;

  localparam logic [11:0] CTRL_OFFSET   = 12'h000;
  localparam logic [11:0] STATUS_OFFSET = 12'h004;
  localparam logic [11:0] DATA0_OFFSET  = 12'h008;
  localparam logic [11:0] DATA1_OFFSET  = 12'h00C;
  localparam logic [11:0] DATA2_OFFSET  = 12'h010;
  localparam logic [11:0] DATA3_OFFSET  = 12'h014;

  typedef enum logic [1:0] {
    IDLE,
    RUN,
    DONE
  } state_e;

  state_e state_q, state_d;

  logic [31:0] data_q [NUM_WORDS];
  logic [31:0] data_d [NUM_WORDS];

  logic [31:0] rdata_d;

  logic [3:0] cycle_cnt_q;
  logic [3:0] cycle_cnt_d;

  logic start_pulse;
  logic clear_done;
  logic busy;
  logic done;

  logic [31:0] u;
  logic [31:0] x;
  logic [31:0] s;

  logic [31:0] mont_z;
  logic [31:0] mont_w;
  logic [31:0] mont_t;
  logic [31:0] v_mont;

  logic [31:0] add_tmp;
  logic [31:0] add_res;
  logic [31:0] sub_res;

  logic unused_wstrb;
  assign unused_wstrb = ^reg_req_i.wstrb;

  assign start_pulse = reg_req_i.valid &&
                       reg_req_i.write &&
                       (reg_req_i.addr[11:0] == CTRL_OFFSET) &&
                       reg_req_i.wdata[0];

  assign clear_done = reg_req_i.valid &&
                      reg_req_i.write &&
                      (reg_req_i.addr[11:0] == CTRL_OFFSET) &&
                      reg_req_i.wdata[1];

  assign busy = (state_q == RUN);
  assign done = (state_q == DONE);

  always_comb begin
    u = data_q[0];
    x = data_q[1];
    s = data_q[2];

    mont_z = x * s;
    mont_w = ((mont_z * Q0I) & 32'h0000FFFF) * Q;
    mont_t = (mont_z + mont_w) >> 16;

    if (mont_t >= Q) begin
      v_mont = mont_t - Q;
    end else begin
      v_mont = mont_t;
    end

    add_tmp = u + v_mont;

    if (add_tmp >= Q) begin
      add_res = add_tmp - Q;
    end else begin
      add_res = add_tmp;
    end

    if (u >= v_mont) begin
      sub_res = u - v_mont;
    end else begin
      sub_res = u + Q - v_mont;
    end
  end

  always_comb begin
    state_d     = state_q;
    cycle_cnt_d = cycle_cnt_q;

    for (int i = 0; i < NUM_WORDS; i++) begin
      data_d[i] = data_q[i];
    end

    unique case (state_q)
      IDLE: begin
        cycle_cnt_d = 4'd0;

        if (start_pulse) begin
          state_d = RUN;
        end
      end

      RUN: begin
        if (cycle_cnt_q == LATENCY_CYCLES[3:0] - 1) begin
          data_d[0] = add_res;
          data_d[1] = sub_res;
          data_d[2] = v_mont;

          cycle_cnt_d = 4'd0;
          state_d     = DONE;
        end else begin
          cycle_cnt_d = cycle_cnt_q + 4'd1;
        end
      end

      DONE: begin
        if (clear_done) begin
          state_d = IDLE;
        end else if (start_pulse) begin
          state_d = RUN;
        end
      end

      default: begin
        state_d     = IDLE;
        cycle_cnt_d = 4'd0;
      end
    endcase

    if (reg_req_i.valid && reg_req_i.write) begin
      unique case (reg_req_i.addr[11:0])
        DATA0_OFFSET: data_d[0] = reg_req_i.wdata;
        DATA1_OFFSET: data_d[1] = reg_req_i.wdata;
        DATA2_OFFSET: data_d[2] = reg_req_i.wdata;
        DATA3_OFFSET: data_d[3] = reg_req_i.wdata;
        default: begin
        end
      endcase
    end
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      state_q     <= IDLE;
      cycle_cnt_q <= 4'd0;

      for (int i = 0; i < NUM_WORDS; i++) begin
        data_q[i] <= 32'h0;
      end
    end else begin
      state_q     <= state_d;
      cycle_cnt_q <= cycle_cnt_d;

      for (int i = 0; i < NUM_WORDS; i++) begin
        data_q[i] <= data_d[i];
      end
    end
  end

  always_comb begin
    rdata_d = 32'h0;

    unique case (reg_req_i.addr[11:0])
      CTRL_OFFSET: begin
        rdata_d = 32'h0;
      end

      STATUS_OFFSET: begin
        rdata_d = {
          30'b0,
          busy,
          done
        };
      end

      DATA0_OFFSET: begin
        rdata_d = data_q[0];
      end

      DATA1_OFFSET: begin
        rdata_d = data_q[1];
      end

      DATA2_OFFSET: begin
        rdata_d = data_q[2];
      end

      DATA3_OFFSET: begin
        rdata_d = data_q[3];
      end

      default: begin
        rdata_d = 32'h0;
      end
    endcase
  end

  assign reg_rsp_o.ready = reg_req_i.valid;
  assign reg_rsp_o.error = 1'b0;
  assign reg_rsp_o.rdata = rdata_d;

endmodule
