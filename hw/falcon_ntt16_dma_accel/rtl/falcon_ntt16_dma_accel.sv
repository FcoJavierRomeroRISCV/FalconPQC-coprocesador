module falcon_ntt16_dma_accel (
    input logic clk_i,
    input logic rst_ni,

    output logic fifo_req_done,
    input  dma_fifo_pkg::fifo_req_t  fifo_req_i,
    output dma_fifo_pkg::fifo_resp_t fifo_resp_o
);

  localparam int unsigned N = 16;

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

  logic [3:0] in_count_q, in_count_d;
  logic [3:0] out_count_q, out_count_d;

  logic output_valid_q, output_valid_d;

  logic [31:0] fifo_data_q;
  logic [31:0] fifo_data_d;

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

  task automatic ntt16_compute(inout logic [31:0] x [0:N-1]);
    begin
      /*
       * Mini-NTT16 siguiendo mq_NTT.
       *
       * GMb[1..15]:
       * 7888, 11060, 11208, 6960, 4342, 6275, 9759,
       * 1591, 6399, 9477, 5266, 586, 5825, 7538, 9710
       */

      // m = 1, t = 16, ht = 8, s = GMb[1]
      butterfly(x[0], x[8],  32'd7888);
      butterfly(x[1], x[9],  32'd7888);
      butterfly(x[2], x[10], 32'd7888);
      butterfly(x[3], x[11], 32'd7888);
      butterfly(x[4], x[12], 32'd7888);
      butterfly(x[5], x[13], 32'd7888);
      butterfly(x[6], x[14], 32'd7888);
      butterfly(x[7], x[15], 32'd7888);

      // m = 2, t = 8, ht = 4, s = GMb[2], GMb[3]
      butterfly(x[0],  x[4],  32'd11060);
      butterfly(x[1],  x[5],  32'd11060);
      butterfly(x[2],  x[6],  32'd11060);
      butterfly(x[3],  x[7],  32'd11060);

      butterfly(x[8],  x[12], 32'd11208);
      butterfly(x[9],  x[13], 32'd11208);
      butterfly(x[10], x[14], 32'd11208);
      butterfly(x[11], x[15], 32'd11208);

      // m = 4, t = 4, ht = 2, s = GMb[4..7]
      butterfly(x[0],  x[2],  32'd6960);
      butterfly(x[1],  x[3],  32'd6960);

      butterfly(x[4],  x[6],  32'd4342);
      butterfly(x[5],  x[7],  32'd4342);

      butterfly(x[8],  x[10], 32'd6275);
      butterfly(x[9],  x[11], 32'd6275);

      butterfly(x[12], x[14], 32'd9759);
      butterfly(x[13], x[15], 32'd9759);

      // m = 8, t = 2, ht = 1, s = GMb[8..15]
      butterfly(x[0],  x[1],  32'd1591);
      butterfly(x[2],  x[3],  32'd6399);
      butterfly(x[4],  x[5],  32'd9477);
      butterfly(x[6],  x[7],  32'd5266);
      butterfly(x[8],  x[9],  32'd586);
      butterfly(x[10], x[11], 32'd5825);
      butterfly(x[12], x[13], 32'd7538);
      butterfly(x[14], x[15], 32'd9710);
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
        out_count_d    = 4'd0;

        if (fifo_req_i.push && !fifo_resp_o.full) begin
          a_d[in_count_q] = fifo_req_i.data;

          if (in_count_q == 4'd15) begin
            in_count_d = 4'd0;
            state_d    = S_COMPUTE;
          end else begin
            in_count_d = in_count_q + 4'd1;
          end
        end
      end

      S_COMPUTE: begin
        ntt16_compute(a_d);

        fifo_data_d    = a_d[0];
        output_valid_d = 1'b1;
        out_count_d    = 4'd0;
        state_d        = S_OUTPUT;
      end

      S_OUTPUT: begin
        fifo_data_d    = a_q[out_count_q];
        output_valid_d = 1'b1;

        if (fifo_req_i.pop && output_valid_q) begin
          if (out_count_q == 4'd15) begin
            out_count_d    = 4'd0;
            output_valid_d = 1'b0;
            state_d        = S_INPUT;
          end else begin
            out_count_d = out_count_q + 4'd1;
            fifo_data_d = a_q[out_count_q + 4'd1];
          end
        end
      end

      default: begin
        state_d = S_INPUT;
      end

    endcase

    if (fifo_req_i.flush) begin
      state_d        = S_INPUT;
      in_count_d     = 4'd0;
      out_count_d    = 4'd0;
      output_valid_d = 1'b0;
      fifo_data_d    = 32'd0;
    end
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      state_q        <= S_INPUT;
      in_count_q     <= 4'd0;
      out_count_q    <= 4'd0;
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

