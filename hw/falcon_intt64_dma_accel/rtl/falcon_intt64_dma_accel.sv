module falcon_intt64_dma_accel (
    input logic clk_i,
    input logic rst_ni,

    output logic fifo_req_done,
    input  dma_fifo_pkg::fifo_req_t  fifo_req_i,
    output dma_fifo_pkg::fifo_resp_t fifo_resp_o
);

  localparam int unsigned N = 64;

  localparam logic [31:0] Q    = 32'd12289;
  localparam logic [31:0] Q0I  = 32'd12287;
  localparam logic [31:0] NI64 = 32'd1024;  // R / 64 mod q, R = 4091

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

  function automatic logic [31:0] get_igmb(input int unsigned idx);
    begin
      unique case (idx)
        1:  get_igmb = 32'd4401;
        2:  get_igmb = 32'd1081;
        3:  get_igmb = 32'd1229;
        4:  get_igmb = 32'd2530;
        5:  get_igmb = 32'd6014;
        6:  get_igmb = 32'd7947;
        7:  get_igmb = 32'd5329;
        8:  get_igmb = 32'd2579;
        9:  get_igmb = 32'd4751;
        10: get_igmb = 32'd6464;
        11: get_igmb = 32'd11703;
        12: get_igmb = 32'd7023;
        13: get_igmb = 32'd2812;
        14: get_igmb = 32'd5890;
        15: get_igmb = 32'd10698;
        16: get_igmb = 32'd3109;
        17: get_igmb = 32'd2125;
        18: get_igmb = 32'd1960;
        19: get_igmb = 32'd10925;
        20: get_igmb = 32'd10601;
        21: get_igmb = 32'd10404;
        22: get_igmb = 32'd4189;
        23: get_igmb = 32'd1875;
        24: get_igmb = 32'd5847;
        25: get_igmb = 32'd8546;
        26: get_igmb = 32'd4615;
        27: get_igmb = 32'd5190;
        28: get_igmb = 32'd11324;
        29: get_igmb = 32'd10578;
        30: get_igmb = 32'd5882;
        31: get_igmb = 32'd11155;

        32: get_igmb = 32'd8417;
        33: get_igmb = 32'd12275;
        34: get_igmb = 32'd10599;
        35: get_igmb = 32'd7446;
        36: get_igmb = 32'd5719;
        37: get_igmb = 32'd3569;
        38: get_igmb = 32'd5981;
        39: get_igmb = 32'd10108;
        40: get_igmb = 32'd4426;
        41: get_igmb = 32'd8306;
        42: get_igmb = 32'd10755;
        43: get_igmb = 32'd4679;
        44: get_igmb = 32'd11052;
        45: get_igmb = 32'd1538;
        46: get_igmb = 32'd11857;
        47: get_igmb = 32'd100;
        48: get_igmb = 32'd8247;
        49: get_igmb = 32'd6625;
        50: get_igmb = 32'd9725;
        51: get_igmb = 32'd5145;
        52: get_igmb = 32'd3412;
        53: get_igmb = 32'd7858;
        54: get_igmb = 32'd5831;
        55: get_igmb = 32'd9460;
        56: get_igmb = 32'd5217;
        57: get_igmb = 32'd10740;
        58: get_igmb = 32'd7882;
        59: get_igmb = 32'd7506;
        60: get_igmb = 32'd12172;
        61: get_igmb = 32'd11292;
        62: get_igmb = 32'd6049;
        63: get_igmb = 32'd79;

        default: get_igmb = 32'd0;
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

  task automatic intt_butterfly(
      inout logic [31:0] x0,
      inout logic [31:0] x1,
      input logic [31:0] s
  );
    logic [31:0] u;
    logic [31:0] v;
    logic [31:0] w;
    begin
      u  = x0;
      v  = x1;
      x0 = mod_add(u, v);
      w  = mod_sub(u, v);
      x1 = montgomery_mul(w, s);
    end
  endtask

  task automatic scale_final(inout logic [31:0] x [0:N-1]);
    begin
      for (int i = 0; i < N; i++) begin
        x[i] = montgomery_mul(x[i], NI64);
      end
    end
  endtask

  task automatic intt64_compute(inout logic [31:0] x [0:N-1]);
    int unsigned m;
    int unsigned hm;
    int unsigned t;
    int unsigned i;
    int unsigned j;
    int unsigned j1;
    int unsigned j2;
    logic [31:0] s;
    begin
      t = 1;

      for (m = N; m > 1; m = m >> 1) begin
        hm = m >> 1;

        for (i = 0; i < hm; i = i + 1) begin
          j1 = i * (t << 1);
          j2 = j1 + t;
          s  = get_igmb(hm + i);

          for (j = j1; j < j2; j = j + 1) begin
            intt_butterfly(x[j], x[j + t], s);
          end
        end

        t = t << 1;
      end

      scale_final(x);
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
        intt64_compute(a_d);

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

