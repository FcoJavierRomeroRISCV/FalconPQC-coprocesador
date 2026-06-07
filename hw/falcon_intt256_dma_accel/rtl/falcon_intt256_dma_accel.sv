module falcon_intt256_dma_accel (
    input logic clk_i,
    input logic rst_ni,

    output logic fifo_req_done,
    input  dma_fifo_pkg::fifo_req_t  fifo_req_i,
    output dma_fifo_pkg::fifo_resp_t fifo_resp_o
);

  localparam int unsigned N = 256;

  localparam logic [31:0] Q     = 32'd12289;
  localparam logic [31:0] Q0I   = 32'd12287;
  localparam logic [31:0] NI256 = 32'd256;

  typedef enum logic [2:0] {
    S_INPUT,
    S_COMPUTE,
    S_OUTPUT
  } state_t;

  state_t state_q, state_d;

  logic [31:0] a_q [0:N-1];
  logic [31:0] a_d [0:N-1];

  logic [7:0] in_count_q, in_count_d;
  logic [7:0] out_count_q, out_count_d;

  logic output_valid_q, output_valid_d;

  logic [31:0] fifo_data_q;
  logic [31:0] fifo_data_d;

  function automatic logic [31:0] get_igmb(input int unsigned idx);
    begin
      unique case (idx)
        1: get_igmb = 32'd4401;
        2: get_igmb = 32'd1081;
        3: get_igmb = 32'd1229;
        4: get_igmb = 32'd2530;
        5: get_igmb = 32'd6014;
        6: get_igmb = 32'd7947;
        7: get_igmb = 32'd5329;
        8: get_igmb = 32'd2579;
        9: get_igmb = 32'd4751;
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
        64: get_igmb = 32'd13;
        65: get_igmb = 32'd6938;
        66: get_igmb = 32'd8886;
        67: get_igmb = 32'd5453;
        68: get_igmb = 32'd4586;
        69: get_igmb = 32'd11455;
        70: get_igmb = 32'd2903;
        71: get_igmb = 32'd4676;
        72: get_igmb = 32'd9843;
        73: get_igmb = 32'd7621;
        74: get_igmb = 32'd8822;
        75: get_igmb = 32'd9109;
        76: get_igmb = 32'd2083;
        77: get_igmb = 32'd8507;
        78: get_igmb = 32'd8685;
        79: get_igmb = 32'd3110;
        80: get_igmb = 32'd7015;
        81: get_igmb = 32'd3269;
        82: get_igmb = 32'd1367;
        83: get_igmb = 32'd6397;
        84: get_igmb = 32'd10259;
        85: get_igmb = 32'd8435;
        86: get_igmb = 32'd10527;
        87: get_igmb = 32'd11559;
        88: get_igmb = 32'd11094;
        89: get_igmb = 32'd2211;
        90: get_igmb = 32'd1808;
        91: get_igmb = 32'd7319;
        92: get_igmb = 32'd48;
        93: get_igmb = 32'd9547;
        94: get_igmb = 32'd2560;
        95: get_igmb = 32'd1228;
        96: get_igmb = 32'd9438;
        97: get_igmb = 32'd10787;
        98: get_igmb = 32'd11800;
        99: get_igmb = 32'd1820;
        100: get_igmb = 32'd11406;
        101: get_igmb = 32'd8966;
        102: get_igmb = 32'd6159;
        103: get_igmb = 32'd3012;
        104: get_igmb = 32'd6109;
        105: get_igmb = 32'd2796;
        106: get_igmb = 32'd2203;
        107: get_igmb = 32'd1652;
        108: get_igmb = 32'd711;
        109: get_igmb = 32'd7004;
        110: get_igmb = 32'd1053;
        111: get_igmb = 32'd8973;
        112: get_igmb = 32'd5244;
        113: get_igmb = 32'd1517;
        114: get_igmb = 32'd9322;
        115: get_igmb = 32'd11269;
        116: get_igmb = 32'd900;
        117: get_igmb = 32'd3888;
        118: get_igmb = 32'd11133;
        119: get_igmb = 32'd10736;
        120: get_igmb = 32'd4949;
        121: get_igmb = 32'd7616;
        122: get_igmb = 32'd9974;
        123: get_igmb = 32'd4746;
        124: get_igmb = 32'd10270;
        125: get_igmb = 32'd126;
        126: get_igmb = 32'd2921;
        127: get_igmb = 32'd6720;
        128: get_igmb = 32'd6635;
        129: get_igmb = 32'd6543;
        130: get_igmb = 32'd1582;
        131: get_igmb = 32'd4868;
        132: get_igmb = 32'd42;
        133: get_igmb = 32'd673;
        134: get_igmb = 32'd2240;
        135: get_igmb = 32'd7219;
        136: get_igmb = 32'd1296;
        137: get_igmb = 32'd11989;
        138: get_igmb = 32'd7675;
        139: get_igmb = 32'd8578;
        140: get_igmb = 32'd11949;
        141: get_igmb = 32'd989;
        142: get_igmb = 32'd10541;
        143: get_igmb = 32'd7687;
        144: get_igmb = 32'd7085;
        145: get_igmb = 32'd8487;
        146: get_igmb = 32'd1004;
        147: get_igmb = 32'd10236;
        148: get_igmb = 32'd4703;
        149: get_igmb = 32'd163;
        150: get_igmb = 32'd9143;
        151: get_igmb = 32'd4597;
        152: get_igmb = 32'd6431;
        153: get_igmb = 32'd12052;
        154: get_igmb = 32'd2991;
        155: get_igmb = 32'd11938;
        156: get_igmb = 32'd4647;
        157: get_igmb = 32'd3362;
        158: get_igmb = 32'd2060;
        159: get_igmb = 32'd11357;
        160: get_igmb = 32'd12011;
        161: get_igmb = 32'd6664;
        162: get_igmb = 32'd5655;
        163: get_igmb = 32'd7225;
        164: get_igmb = 32'd5914;
        165: get_igmb = 32'd9327;
        166: get_igmb = 32'd4092;
        167: get_igmb = 32'd5880;
        168: get_igmb = 32'd6932;
        169: get_igmb = 32'd3402;
        170: get_igmb = 32'd5133;
        171: get_igmb = 32'd9394;
        172: get_igmb = 32'd11229;
        173: get_igmb = 32'd5252;
        174: get_igmb = 32'd9008;
        175: get_igmb = 32'd1556;
        176: get_igmb = 32'd6908;
        177: get_igmb = 32'd4773;
        178: get_igmb = 32'd3853;
        179: get_igmb = 32'd8780;
        180: get_igmb = 32'd10325;
        181: get_igmb = 32'd7737;
        182: get_igmb = 32'd1758;
        183: get_igmb = 32'd7103;
        184: get_igmb = 32'd11375;
        185: get_igmb = 32'd12273;
        186: get_igmb = 32'd8602;
        187: get_igmb = 32'd3243;
        188: get_igmb = 32'd6536;
        189: get_igmb = 32'd7590;
        190: get_igmb = 32'd8591;
        191: get_igmb = 32'd11552;
        192: get_igmb = 32'd6101;
        193: get_igmb = 32'd3253;
        194: get_igmb = 32'd9969;
        195: get_igmb = 32'd9640;
        196: get_igmb = 32'd4506;
        197: get_igmb = 32'd3736;
        198: get_igmb = 32'd6829;
        199: get_igmb = 32'd10822;
        200: get_igmb = 32'd9130;
        201: get_igmb = 32'd9948;
        202: get_igmb = 32'd3566;
        203: get_igmb = 32'd2133;
        204: get_igmb = 32'd3901;
        205: get_igmb = 32'd6038;
        206: get_igmb = 32'd7333;
        207: get_igmb = 32'd6609;
        208: get_igmb = 32'd3468;
        209: get_igmb = 32'd4659;
        210: get_igmb = 32'd625;
        211: get_igmb = 32'd2700;
        212: get_igmb = 32'd7738;
        213: get_igmb = 32'd3443;
        214: get_igmb = 32'd3060;
        215: get_igmb = 32'd3388;
        216: get_igmb = 32'd3526;
        217: get_igmb = 32'd4418;
        218: get_igmb = 32'd11911;
        219: get_igmb = 32'd6232;
        220: get_igmb = 32'd1730;
        221: get_igmb = 32'd2558;
        222: get_igmb = 32'd10340;
        223: get_igmb = 32'd5344;
        224: get_igmb = 32'd5286;
        225: get_igmb = 32'd2190;
        226: get_igmb = 32'd11562;
        227: get_igmb = 32'd6199;
        228: get_igmb = 32'd2482;
        229: get_igmb = 32'd8756;
        230: get_igmb = 32'd5387;
        231: get_igmb = 32'd4101;
        232: get_igmb = 32'd4609;
        233: get_igmb = 32'd8605;
        234: get_igmb = 32'd8226;
        235: get_igmb = 32'd144;
        236: get_igmb = 32'd5656;
        237: get_igmb = 32'd8704;
        238: get_igmb = 32'd2621;
        239: get_igmb = 32'd5424;
        240: get_igmb = 32'd10812;
        241: get_igmb = 32'd2959;
        242: get_igmb = 32'd11346;
        243: get_igmb = 32'd6249;
        244: get_igmb = 32'd1715;
        245: get_igmb = 32'd4951;
        246: get_igmb = 32'd9540;
        247: get_igmb = 32'd1888;
        248: get_igmb = 32'd3764;
        249: get_igmb = 32'd39;
        250: get_igmb = 32'd8219;
        251: get_igmb = 32'd2080;
        252: get_igmb = 32'd2502;
        253: get_igmb = 32'd1469;
        254: get_igmb = 32'd10550;
        255: get_igmb = 32'd8709;
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
        x[i] = montgomery_mul(x[i], NI256);
      end
    end
  endtask

  task automatic intt256_compute(inout logic [31:0] x [0:N-1]);
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
        out_count_d    = 8'd0;

        if (fifo_req_i.push && !fifo_resp_o.full) begin
          a_d[in_count_q] = fifo_req_i.data;

          if (in_count_q == 8'd255) begin
            in_count_d = 8'd0;
            state_d    = S_COMPUTE;
          end else begin
            in_count_d = in_count_q + 8'd1;
          end
        end
      end

      S_COMPUTE: begin
        intt256_compute(a_d);

        fifo_data_d    = a_d[0];
        output_valid_d = 1'b1;
        out_count_d    = 8'd0;
        state_d        = S_OUTPUT;
      end

      S_OUTPUT: begin
        fifo_data_d    = a_q[out_count_q];
        output_valid_d = 1'b1;

        if (fifo_req_i.pop && output_valid_q) begin
          if (out_count_q == 8'd255) begin
            out_count_d    = 8'd0;
            output_valid_d = 1'b0;
            state_d        = S_INPUT;
          end else begin
            out_count_d = out_count_q + 8'd1;
            fifo_data_d = a_q[out_count_q + 8'd1];
          end
        end
      end

      default: begin
        state_d = S_INPUT;
      end

    endcase

    if (fifo_req_i.flush) begin
      state_d        = S_INPUT;
      in_count_d     = 8'd0;
      out_count_d    = 8'd0;
      output_valid_d = 1'b0;
      fifo_data_d    = 32'd0;
    end
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      state_q        <= S_INPUT;
      in_count_q     <= 8'd0;
      out_count_q    <= 8'd0;
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
