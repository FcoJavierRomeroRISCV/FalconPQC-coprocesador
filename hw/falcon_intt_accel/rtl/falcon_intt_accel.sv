module falcon_intt_accel (
    input  logic clk_i,
    input  logic rst_ni,

    input  reg_pkg::reg_req_t reg_req_i,
    output reg_pkg::reg_rsp_t reg_rsp_o
);

  localparam int unsigned BUFFER_WORDS = 16;
  localparam int unsigned LATENCY_CYCLES = 8;

  localparam logic [31:0] Q   = 32'd12289;
  localparam logic [31:0] Q0I = 32'd12287;

  localparam logic [31:0] IGMB_1 = 32'd4401;
  localparam logic [31:0] IGMB_2 = 32'd1081;
  localparam logic [31:0] IGMB_3 = 32'd1229;
  localparam logic [31:0] IGMB_4 = 32'd2530;
  localparam logic [31:0] IGMB_5 = 32'd6014;
  localparam logic [31:0] IGMB_6 = 32'd7947;
  localparam logic [31:0] IGMB_7 = 32'd5329;

  localparam logic [31:0] NI_N8 = 32'd8192;

  localparam logic [11:0] CTRL_OFFSET        = 12'h000;
  localparam logic [11:0] STATUS_OFFSET      = 12'h004;
  localparam logic [11:0] ADDR_OFFSET        = 12'h008;
  localparam logic [11:0] WDATA_OFFSET       = 12'h00C;
  localparam logic [11:0] RDATA_OFFSET       = 12'h010;
  localparam logic [11:0] SIZE_OFFSET        = 12'h014;
  localparam logic [11:0] PERF_CYCLES_OFFSET = 12'h018;

  localparam logic [31:0] CTRL_START        = 32'h00000001;
  localparam logic [31:0] CTRL_CLEAR_DONE   = 32'h00000002;
  localparam logic [31:0] CTRL_BUFFER_WRITE = 32'h00000004;
  localparam logic [31:0] CTRL_BUFFER_READ  = 32'h00000008;

  typedef enum logic [1:0] {
    IDLE,
    RUN,
    DONE
  } state_e;

  state_e state_q, state_d;

  logic [31:0] buffer_q [BUFFER_WORDS];
  logic [31:0] buffer_d [BUFFER_WORDS];

  logic [31:0] addr_q;
  logic [31:0] addr_d;

  logic [31:0] wdata_q;
  logic [31:0] wdata_d;

  logic [31:0] rdata_q;
  logic [31:0] rdata_d;

  logic [31:0] reg_rdata_d;

  logic [3:0] cycle_cnt_q;
  logic [3:0] cycle_cnt_d;

  logic [31:0] perf_cycles_q;
  logic [31:0] perf_cycles_d;

  logic start_pulse;
  logic clear_done;
  logic buffer_write_pulse;
  logic buffer_read_pulse;

  logic busy;
  logic done;

  logic addr_valid;

  logic unused_wstrb;
  assign unused_wstrb = ^reg_req_i.wstrb;

  assign start_pulse = reg_req_i.valid &&
                       reg_req_i.write &&
                       (reg_req_i.addr[11:0] == CTRL_OFFSET) &&
                       ((reg_req_i.wdata & CTRL_START) != 32'h0);

  assign clear_done = reg_req_i.valid &&
                      reg_req_i.write &&
                      (reg_req_i.addr[11:0] == CTRL_OFFSET) &&
                      ((reg_req_i.wdata & CTRL_CLEAR_DONE) != 32'h0);

  assign buffer_write_pulse = reg_req_i.valid &&
                              reg_req_i.write &&
                              (reg_req_i.addr[11:0] == CTRL_OFFSET) &&
                              ((reg_req_i.wdata & CTRL_BUFFER_WRITE) != 32'h0);

  assign buffer_read_pulse = reg_req_i.valid &&
                             reg_req_i.write &&
                             (reg_req_i.addr[11:0] == CTRL_OFFSET) &&
                             ((reg_req_i.wdata & CTRL_BUFFER_READ) != 32'h0);

  assign busy = (state_q == RUN);
  assign done = (state_q == DONE);

  assign addr_valid = (addr_q < BUFFER_WORDS);

  function automatic logic [31:0] mq_add(
      input logic [31:0] a,
      input logic [31:0] b
  );
    logic [31:0] tmp;
    begin
      tmp = a + b;
      if (tmp >= Q) begin
        mq_add = tmp - Q;
      end else begin
        mq_add = tmp;
      end
    end
  endfunction

  function automatic logic [31:0] mq_sub(
      input logic [31:0] a,
      input logic [31:0] b
  );
    begin
      if (a >= b) begin
        mq_sub = a - b;
      end else begin
        mq_sub = a + Q - b;
      end
    end
  endfunction

  function automatic logic [31:0] mq_montymul(
      input logic [31:0] x,
      input logic [31:0] y
  );
    logic [31:0] z;
    logic [31:0] w;
    logic [31:0] t;
    begin
      z = x * y;
      w = ((z * Q0I) & 32'h0000FFFF) * Q;
      t = (z + w) >> 16;

      if (t >= Q) begin
        mq_montymul = t - Q;
      end else begin
        mq_montymul = t;
      end
    end
  endfunction

  task automatic intt_butterfly(
      input  logic [31:0] in_u,
      input  logic [31:0] in_v,
      input  logic [31:0] in_s,
      output logic [31:0] out_u,
      output logic [31:0] out_v
  );
    logic [31:0] w;
    begin
      out_u = mq_add(in_u, in_v);
      w     = mq_sub(in_u, in_v);
      out_v = mq_montymul(w, in_s);
    end
  endtask

  always_comb begin
    state_d       = state_q;
    cycle_cnt_d   = cycle_cnt_q;
    perf_cycles_d = perf_cycles_q;

    addr_d  = addr_q;
    wdata_d = wdata_q;
    rdata_d = rdata_q;

    for (int i = 0; i < BUFFER_WORDS; i++) begin
      buffer_d[i] = buffer_q[i];
    end

    if (reg_req_i.valid && reg_req_i.write) begin
      unique case (reg_req_i.addr[11:0])
        ADDR_OFFSET: begin
          addr_d = reg_req_i.wdata;
        end

        WDATA_OFFSET: begin
          wdata_d = reg_req_i.wdata;
        end

        default: begin
        end
      endcase
    end

    if (buffer_write_pulse && addr_valid) begin
      buffer_d[addr_q[$clog2(BUFFER_WORDS)-1:0]] = wdata_q;
    end

    if (buffer_read_pulse && addr_valid) begin
      rdata_d = buffer_q[addr_q[$clog2(BUFFER_WORDS)-1:0]];
    end

    unique case (state_q)
      IDLE: begin
        cycle_cnt_d = 4'd0;

        if (start_pulse) begin
          perf_cycles_d = 32'd0;
          state_d       = RUN;
        end
      end

      RUN: begin
        perf_cycles_d = perf_cycles_q + 32'd1;

        if (cycle_cnt_q == LATENCY_CYCLES[3:0] - 1) begin
          logic [31:0] s1_a0;
          logic [31:0] s1_a1;
          logic [31:0] s1_a2;
          logic [31:0] s1_a3;
          logic [31:0] s1_a4;
          logic [31:0] s1_a5;
          logic [31:0] s1_a6;
          logic [31:0] s1_a7;

          logic [31:0] s2_a0;
          logic [31:0] s2_a1;
          logic [31:0] s2_a2;
          logic [31:0] s2_a3;
          logic [31:0] s2_a4;
          logic [31:0] s2_a5;
          logic [31:0] s2_a6;
          logic [31:0] s2_a7;

          logic [31:0] s3_a0;
          logic [31:0] s3_a1;
          logic [31:0] s3_a2;
          logic [31:0] s3_a3;
          logic [31:0] s3_a4;
          logic [31:0] s3_a5;
          logic [31:0] s3_a6;
          logic [31:0] s3_a7;

          // Stage 1: t = 1, m = 8, hm = 4
          intt_butterfly(buffer_q[0], buffer_q[1], IGMB_4, s1_a0, s1_a1);
          intt_butterfly(buffer_q[2], buffer_q[3], IGMB_5, s1_a2, s1_a3);
          intt_butterfly(buffer_q[4], buffer_q[5], IGMB_6, s1_a4, s1_a5);
          intt_butterfly(buffer_q[6], buffer_q[7], IGMB_7, s1_a6, s1_a7);

          // Stage 2: t = 2, m = 4, hm = 2
          intt_butterfly(s1_a0, s1_a2, IGMB_2, s2_a0, s2_a2);
          intt_butterfly(s1_a1, s1_a3, IGMB_2, s2_a1, s2_a3);

          intt_butterfly(s1_a4, s1_a6, IGMB_3, s2_a4, s2_a6);
          intt_butterfly(s1_a5, s1_a7, IGMB_3, s2_a5, s2_a7);

          // Stage 3: t = 4, m = 2, hm = 1
          intt_butterfly(s2_a0, s2_a4, IGMB_1, s3_a0, s3_a4);
          intt_butterfly(s2_a1, s2_a5, IGMB_1, s3_a1, s3_a5);
          intt_butterfly(s2_a2, s2_a6, IGMB_1, s3_a2, s3_a6);
          intt_butterfly(s2_a3, s2_a7, IGMB_1, s3_a3, s3_a7);

          // Final scaling by ni for n = 8.
          buffer_d[0] = mq_montymul(s3_a0, NI_N8);
          buffer_d[1] = mq_montymul(s3_a1, NI_N8);
          buffer_d[2] = mq_montymul(s3_a2, NI_N8);
          buffer_d[3] = mq_montymul(s3_a3, NI_N8);
          buffer_d[4] = mq_montymul(s3_a4, NI_N8);
          buffer_d[5] = mq_montymul(s3_a5, NI_N8);
          buffer_d[6] = mq_montymul(s3_a6, NI_N8);
          buffer_d[7] = mq_montymul(s3_a7, NI_N8);

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
          perf_cycles_d = 32'd0;
          state_d       = RUN;
        end
      end

      default: begin
        state_d       = IDLE;
        cycle_cnt_d   = 4'd0;
        perf_cycles_d = 32'd0;
      end
    endcase
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      state_q       <= IDLE;
      cycle_cnt_q   <= 4'd0;
      perf_cycles_q <= 32'h0;
      addr_q        <= 32'h0;
      wdata_q       <= 32'h0;
      rdata_q       <= 32'h0;

      for (int i = 0; i < BUFFER_WORDS; i++) begin
        buffer_q[i] <= 32'h0;
      end
    end else begin
      state_q       <= state_d;
      cycle_cnt_q   <= cycle_cnt_d;
      perf_cycles_q <= perf_cycles_d;
      addr_q        <= addr_d;
      wdata_q       <= wdata_d;
      rdata_q       <= rdata_d;

      for (int i = 0; i < BUFFER_WORDS; i++) begin
        buffer_q[i] <= buffer_d[i];
      end
    end
  end

  always_comb begin
    reg_rdata_d = 32'h0;

    unique case (reg_req_i.addr[11:0])
      CTRL_OFFSET: begin
        reg_rdata_d = 32'h0;
      end

      STATUS_OFFSET: begin
        reg_rdata_d = {
          30'b0,
          busy,
          done
        };
      end

      ADDR_OFFSET: begin
        reg_rdata_d = addr_q;
      end

      WDATA_OFFSET: begin
        reg_rdata_d = wdata_q;
      end

      RDATA_OFFSET: begin
        reg_rdata_d = rdata_q;
      end

      SIZE_OFFSET: begin
        reg_rdata_d = BUFFER_WORDS;
      end

      PERF_CYCLES_OFFSET: begin
        reg_rdata_d = perf_cycles_q;
      end

      default: begin
        reg_rdata_d = 32'h0;
      end
    endcase
  end

  assign reg_rsp_o.ready = reg_req_i.valid;
  assign reg_rsp_o.error = 1'b0;
  assign reg_rsp_o.rdata = reg_rdata_d;

endmodule
