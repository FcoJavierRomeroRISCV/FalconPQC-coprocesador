module falcon_ntt64_dma_accel (
    input logic clk_i,
    input logic rst_ni,

    output logic fifo_req_done,
    input  dma_fifo_pkg::fifo_req_t  fifo_req_i,
    output dma_fifo_pkg::fifo_resp_t fifo_resp_o
);

  localparam int unsigned N = 64;

  localparam logic [31:0] Q   = 32'd12289;
  localparam logic [31:0] Q0I = 32'd12287;

  typedef enum logic [2:0] {
    S_INPUT,
    S_COMPUTE,
    S_OUTPUT
  } state_t;

  state_t state_q, state_d;

  logic [31:0] a_q [0:N-1];
  logic [31:0] a_d [0:N-1];

  logic [5:0] in_count_q, in_count_d;
  logic [5:0] out_count_q, out_count_d;

  logic output_valid_q, output_valid_d;

  logic [31:0] fifo_data_q;
  logic [31:0] fifo_data_d;

  function automatic logic [31:0] get_gmb(input int unsigned idx);
    begin
      unique case (idx)
        1:  get_gmb = 32'd7888;
        2:  get_gmb = 32'd11060;
        3:  get_gmb = 32'd11208;
        4:  get_gmb = 32'd6960;
        5:  get_gmb = 32'd4342;
        6:  get_gmb = 32'd6275;
        7:  get_gmb = 32'd9759;
        8:  get_gmb = 32'd1591;
        9:  get_gmb = 32'd6399;
        10: get_gmb = 32'd9477;
        11: get_gmb = 32'd5266;
        12: get_gmb = 32'd586;
        13: get_gmb = 32'd5825;
        14: get_gmb = 32'd7538;
        15: get_gmb = 32'd9710;
        16: get_gmb = 32'd1134;
        17: get_gmb = 32'd6407;
        18: get_gmb = 32'd1711;
        19: get_gmb = 32'd965;
        20: get_gmb = 32'd7099;
        21: get_gmb = 32'd7674;
        22: get_gmb = 32'd3743;
        23: get_gmb = 32'd6442;
        24: get_gmb = 32'd10414;
        25: get_gmb = 32'd8100;
        26: get_gmb = 32'd1885;
        27: get_gmb = 32'd1688;
        28: get_gmb = 32'd1364;
        29: get_gmb = 32'd10329;
        30: get_gmb = 32'd10164;
        31: get_gmb = 32'd9180;
        32: get_gmb = 32'd12210;
        33: get_gmb = 32'd6240;
        34: get_gmb = 32'd997;
        35: get_gmb = 32'd117;
        36: get_gmb = 32'd4783;
        37: get_gmb = 32'd4407;
        38: get_gmb = 32'd1549;
        39: get_gmb = 32'd7072;
        40: get_gmb = 32'd2829;
        41: get_gmb = 32'd6458;
        42: get_gmb = 32'd4431;
        43: get_gmb = 32'd8877;
        44: get_gmb = 32'd7144;
        45: get_gmb = 32'd2564;
        46: get_gmb = 32'd5664;
        47: get_gmb = 32'd4042;
        48: get_gmb = 32'd12189;
        49: get_gmb = 32'd432;
        50: get_gmb = 32'd10751;
        51: get_gmb = 32'd1237;
        52: get_gmb = 32'd7610;
        53: get_gmb = 32'd1534;
        54: get_gmb = 32'd3983;
        55: get_gmb = 32'd7863;
        56: get_gmb = 32'd2181;
        57: get_gmb = 32'd6308;
        58: get_gmb = 32'd8720;
        59: get_gmb = 32'd6570;
        60: get_gmb = 32'd4843;
        61: get_gmb = 32'd1690;
        62: get_gmb = 32'd14;
        63: get_gmb = 32'd3872;
        default: get_gmb = 32'd0;
      endcase
    end
  endfunction

  function automatic logic [31:0] mod_add(input logic [31:0] u, input logic [31:0] v);
    logic [31:0] tmp;
    begin
      tmp = u + v;
      if (tmp >= Q) begin
        tmp = tmp - Q;
      end
      mod_add = tmp;
    end
  endfunction

  function automatic logic [31:0] mod_sub(input logic [31:0] u, input logic [31:0] v);
    begin
      if (u >= v) begin
        mod_sub = u - v;
      end else begin
        mod_sub = u + Q - v;
      end
    end
  endfunction

  function automatic logic [31:0] montgomery_mul(
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
        t = t - Q;
      end

      montgomery_mul = t;
    end
  endfunction

  task automatic butterfly(
      inout logic [31:0] x0,
      inout logic [31:0] x1,
      input logic [31:0] s
  );
    logic [31:0] u;
    logic [31:0] v;
    begin
      u = x0;
      v = montgomery_mul(x1, s);
      x0 = mod_add(u, v);
      x1 = mod_sub(u, v);
    end
  endtask

  task automatic ntt64_compute(inout logic [31:0] x [0:N-1]);
    int unsigned t;
    int unsigned ht;
    int unsigned m;
    int unsigned i;
    int unsigned j;
    int unsigned j1;
    int unsigned j2;
    logic [31:0] s;
    begin
      t = N;

      for (m = 1; m < N; m = m << 1) begin
        ht = t >> 1;

        for (i = 0; i < m; i = i + 1) begin
          j1 = i * t;
          j2 = j1 + ht;
          s  = get_gmb(m + i);

          for (j = j1; j < j2; j = j + 1) begin
            butterfly(x[j], x[j + ht], s);
          end
        end

        t = ht;
      end
    end
  endtask

  always_comb begin
    state_d        = state_q;
    in_count_d     = in_count_q;
    out_count_d    = out_count_q;
    output_valid_d = output_valid_q;
    fifo_data_d    = fifo_data_q;

    for (int i = 0; i < N; i++) begin
      a_d[i] = a_q[i];
    end

    case (state_q)

      S_INPUT: begin
        output_valid_d = 1'b0;
        out_count_d    = 6'd0;

        if (fifo_req_i.push && !fifo_resp_o.full) begin
          a_d[in_count_q] = fifo_req_i.data;

          if (in_count_q == 6'd63) begin
            in_count_d = 6'd0;
            state_d    = S_COMPUTE;
          end else begin
            in_count_d = in_count_q + 6'd1;
          end
        end
      end

      S_COMPUTE: begin
        ntt64_compute(a_d);

        fifo_data_d    = a_d[0];
        output_valid_d = 1'b1;
        out_count_d    = 6'd0;
        state_d        = S_OUTPUT;
      end

      S_OUTPUT: begin
        fifo_data_d    = a_q[out_count_q];
        output_valid_d = 1'b1;

        if (fifo_req_i.pop && output_valid_q) begin
          if (out_count_q == 6'd63) begin
            out_count_d    = 6'd0;
            output_valid_d = 1'b0;
            state_d        = S_INPUT;
          end else begin
            out_count_d = out_count_q + 6'd1;
            fifo_data_d = a_q[out_count_q + 6'd1];
          end
        end
      end

      default: begin
        state_d = S_INPUT;
      end

    endcase

    if (fifo_req_i.flush) begin
      state_d        = S_INPUT;
      in_count_d     = 6'd0;
      out_count_d    = 6'd0;
      output_valid_d = 1'b0;
      fifo_data_d    = 32'd0;
    end
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      state_q        <= S_INPUT;
      in_count_q     <= 6'd0;
      out_count_q    <= 6'd0;
      output_valid_q <= 1'b0;
      fifo_data_q    <= 32'd0;

      for (int i = 0; i < N; i++) begin
        a_q[i] <= 32'd0;
      end
    end else begin
      state_q        <= state_d;
      in_count_q     <= in_count_d;
      out_count_q    <= out_count_d;
      output_valid_q <= output_valid_d;
      fifo_data_q    <= fifo_data_d;

      for (int i = 0; i < N; i++) begin
        a_q[i] <= a_d[i];
      end
    end
  end

  assign fifo_resp_o.full     = (state_q != S_INPUT);
  assign fifo_resp_o.alm_full = (state_q != S_INPUT);

  assign fifo_resp_o.empty = ~output_valid_q;
  assign fifo_resp_o.data  = fifo_data_q;

  assign fifo_req_done = 1'b0;

endmodule

