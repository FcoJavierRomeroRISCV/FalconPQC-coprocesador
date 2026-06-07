module falcon_ntt256_dma_accel (
    input logic clk_i,
    input logic rst_ni,

    output logic fifo_req_done,
    input  dma_fifo_pkg::fifo_req_t  fifo_req_i,
    output dma_fifo_pkg::fifo_resp_t fifo_resp_o
);

  localparam int unsigned N = 256;

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

  logic [7:0] in_count_q, in_count_d;
  logic [7:0] out_count_q, out_count_d;

  logic output_valid_q, output_valid_d;

  logic [31:0] fifo_data_q;
  logic [31:0] fifo_data_d;

  function automatic logic [31:0] get_gmb(input int unsigned idx);
    begin
      unique case (idx)
        1: get_gmb = 32'd7888;
        2: get_gmb = 32'd11060;
        3: get_gmb = 32'd11208;
        4: get_gmb = 32'd6960;
        5: get_gmb = 32'd4342;
        6: get_gmb = 32'd6275;
        7: get_gmb = 32'd9759;
        8: get_gmb = 32'd1591;
        9: get_gmb = 32'd6399;
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
        64: get_gmb = 32'd5569;
        65: get_gmb = 32'd9368;
        66: get_gmb = 32'd12163;
        67: get_gmb = 32'd2019;
        68: get_gmb = 32'd7543;
        69: get_gmb = 32'd2315;
        70: get_gmb = 32'd4673;
        71: get_gmb = 32'd7340;
        72: get_gmb = 32'd1553;
        73: get_gmb = 32'd1156;
        74: get_gmb = 32'd8401;
        75: get_gmb = 32'd11389;
        76: get_gmb = 32'd1020;
        77: get_gmb = 32'd2967;
        78: get_gmb = 32'd10772;
        79: get_gmb = 32'd7045;
        80: get_gmb = 32'd3316;
        81: get_gmb = 32'd11236;
        82: get_gmb = 32'd5285;
        83: get_gmb = 32'd11578;
        84: get_gmb = 32'd10637;
        85: get_gmb = 32'd10086;
        86: get_gmb = 32'd9493;
        87: get_gmb = 32'd6180;
        88: get_gmb = 32'd9277;
        89: get_gmb = 32'd6130;
        90: get_gmb = 32'd3323;
        91: get_gmb = 32'd883;
        92: get_gmb = 32'd10469;
        93: get_gmb = 32'd489;
        94: get_gmb = 32'd1502;
        95: get_gmb = 32'd2851;
        96: get_gmb = 32'd11061;
        97: get_gmb = 32'd9729;
        98: get_gmb = 32'd2742;
        99: get_gmb = 32'd12241;
        100: get_gmb = 32'd4970;
        101: get_gmb = 32'd10481;
        102: get_gmb = 32'd10078;
        103: get_gmb = 32'd1195;
        104: get_gmb = 32'd730;
        105: get_gmb = 32'd1762;
        106: get_gmb = 32'd3854;
        107: get_gmb = 32'd2030;
        108: get_gmb = 32'd5892;
        109: get_gmb = 32'd10922;
        110: get_gmb = 32'd9020;
        111: get_gmb = 32'd5274;
        112: get_gmb = 32'd9179;
        113: get_gmb = 32'd3604;
        114: get_gmb = 32'd3782;
        115: get_gmb = 32'd10206;
        116: get_gmb = 32'd3180;
        117: get_gmb = 32'd3467;
        118: get_gmb = 32'd4668;
        119: get_gmb = 32'd2446;
        120: get_gmb = 32'd7613;
        121: get_gmb = 32'd9386;
        122: get_gmb = 32'd834;
        123: get_gmb = 32'd7703;
        124: get_gmb = 32'd6836;
        125: get_gmb = 32'd3403;
        126: get_gmb = 32'd5351;
        127: get_gmb = 32'd12276;
        128: get_gmb = 32'd3580;
        129: get_gmb = 32'd1739;
        130: get_gmb = 32'd10820;
        131: get_gmb = 32'd9787;
        132: get_gmb = 32'd10209;
        133: get_gmb = 32'd4070;
        134: get_gmb = 32'd12250;
        135: get_gmb = 32'd8525;
        136: get_gmb = 32'd10401;
        137: get_gmb = 32'd2749;
        138: get_gmb = 32'd7338;
        139: get_gmb = 32'd10574;
        140: get_gmb = 32'd6040;
        141: get_gmb = 32'd943;
        142: get_gmb = 32'd9330;
        143: get_gmb = 32'd1477;
        144: get_gmb = 32'd6865;
        145: get_gmb = 32'd9668;
        146: get_gmb = 32'd3585;
        147: get_gmb = 32'd6633;
        148: get_gmb = 32'd12145;
        149: get_gmb = 32'd4063;
        150: get_gmb = 32'd3684;
        151: get_gmb = 32'd7680;
        152: get_gmb = 32'd8188;
        153: get_gmb = 32'd6902;
        154: get_gmb = 32'd3533;
        155: get_gmb = 32'd9807;
        156: get_gmb = 32'd6090;
        157: get_gmb = 32'd727;
        158: get_gmb = 32'd10099;
        159: get_gmb = 32'd7003;
        160: get_gmb = 32'd6945;
        161: get_gmb = 32'd1949;
        162: get_gmb = 32'd9731;
        163: get_gmb = 32'd10559;
        164: get_gmb = 32'd6057;
        165: get_gmb = 32'd378;
        166: get_gmb = 32'd7871;
        167: get_gmb = 32'd8763;
        168: get_gmb = 32'd8901;
        169: get_gmb = 32'd9229;
        170: get_gmb = 32'd8846;
        171: get_gmb = 32'd4551;
        172: get_gmb = 32'd9589;
        173: get_gmb = 32'd11664;
        174: get_gmb = 32'd7630;
        175: get_gmb = 32'd8821;
        176: get_gmb = 32'd5680;
        177: get_gmb = 32'd4956;
        178: get_gmb = 32'd6251;
        179: get_gmb = 32'd8388;
        180: get_gmb = 32'd10156;
        181: get_gmb = 32'd8723;
        182: get_gmb = 32'd2341;
        183: get_gmb = 32'd3159;
        184: get_gmb = 32'd1467;
        185: get_gmb = 32'd5460;
        186: get_gmb = 32'd8553;
        187: get_gmb = 32'd7783;
        188: get_gmb = 32'd2649;
        189: get_gmb = 32'd2320;
        190: get_gmb = 32'd9036;
        191: get_gmb = 32'd6188;
        192: get_gmb = 32'd737;
        193: get_gmb = 32'd3698;
        194: get_gmb = 32'd4699;
        195: get_gmb = 32'd5753;
        196: get_gmb = 32'd9046;
        197: get_gmb = 32'd3687;
        198: get_gmb = 32'd16;
        199: get_gmb = 32'd914;
        200: get_gmb = 32'd5186;
        201: get_gmb = 32'd10531;
        202: get_gmb = 32'd4552;
        203: get_gmb = 32'd1964;
        204: get_gmb = 32'd3509;
        205: get_gmb = 32'd8436;
        206: get_gmb = 32'd7516;
        207: get_gmb = 32'd5381;
        208: get_gmb = 32'd10733;
        209: get_gmb = 32'd3281;
        210: get_gmb = 32'd7037;
        211: get_gmb = 32'd1060;
        212: get_gmb = 32'd2895;
        213: get_gmb = 32'd7156;
        214: get_gmb = 32'd8887;
        215: get_gmb = 32'd5357;
        216: get_gmb = 32'd6409;
        217: get_gmb = 32'd8197;
        218: get_gmb = 32'd2962;
        219: get_gmb = 32'd6375;
        220: get_gmb = 32'd5064;
        221: get_gmb = 32'd6634;
        222: get_gmb = 32'd5625;
        223: get_gmb = 32'd278;
        224: get_gmb = 32'd932;
        225: get_gmb = 32'd10229;
        226: get_gmb = 32'd8927;
        227: get_gmb = 32'd7642;
        228: get_gmb = 32'd351;
        229: get_gmb = 32'd9298;
        230: get_gmb = 32'd237;
        231: get_gmb = 32'd5858;
        232: get_gmb = 32'd7692;
        233: get_gmb = 32'd3146;
        234: get_gmb = 32'd12126;
        235: get_gmb = 32'd7586;
        236: get_gmb = 32'd2053;
        237: get_gmb = 32'd11285;
        238: get_gmb = 32'd3802;
        239: get_gmb = 32'd5204;
        240: get_gmb = 32'd4602;
        241: get_gmb = 32'd1748;
        242: get_gmb = 32'd11300;
        243: get_gmb = 32'd340;
        244: get_gmb = 32'd3711;
        245: get_gmb = 32'd4614;
        246: get_gmb = 32'd300;
        247: get_gmb = 32'd10993;
        248: get_gmb = 32'd5070;
        249: get_gmb = 32'd10049;
        250: get_gmb = 32'd11616;
        251: get_gmb = 32'd12247;
        252: get_gmb = 32'd7421;
        253: get_gmb = 32'd10707;
        254: get_gmb = 32'd5746;
        255: get_gmb = 32'd5654;
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

  task automatic ntt256_compute(inout logic [31:0] x [0:N-1]);
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
        ntt256_compute(a_d);

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
