module falcon_intt1024_dma_accel (
    input logic clk_i,
    input logic rst_ni,

    output logic fifo_req_done,
    input  dma_fifo_pkg::fifo_req_t  fifo_req_i,
    output dma_fifo_pkg::fifo_resp_t fifo_resp_o
);

  localparam int unsigned N = 1024;

  localparam logic [31:0] Q      = 32'd12289;
  localparam logic [31:0] Q0I    = 32'd12287;
  localparam logic [31:0] NI1024 = 32'd12277;

  typedef enum logic [2:0] {
    S_INPUT,
    S_COMPUTE,
    S_OUTPUT
  } state_t;

  state_t state_q, state_d;

  logic [31:0] a_q [0:N-1];
  logic [31:0] a_d [0:N-1];

  logic [9:0] in_count_q, in_count_d;
  logic [9:0] out_count_q, out_count_d;

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
        256: get_igmb = 32'd5601;
        257: get_igmb = 32'd1093;
        258: get_igmb = 32'd3784;
        259: get_igmb = 32'd5041;
        260: get_igmb = 32'd2058;
        261: get_igmb = 32'd8399;
        262: get_igmb = 32'd11448;
        263: get_igmb = 32'd9639;
        264: get_igmb = 32'd2059;
        265: get_igmb = 32'd9878;
        266: get_igmb = 32'd7405;
        267: get_igmb = 32'd2496;
        268: get_igmb = 32'd7918;
        269: get_igmb = 32'd11594;
        270: get_igmb = 32'd371;
        271: get_igmb = 32'd7993;
        272: get_igmb = 32'd3073;
        273: get_igmb = 32'd10326;
        274: get_igmb = 32'd40;
        275: get_igmb = 32'd10004;
        276: get_igmb = 32'd9245;
        277: get_igmb = 32'd7987;
        278: get_igmb = 32'd5603;
        279: get_igmb = 32'd4051;
        280: get_igmb = 32'd7894;
        281: get_igmb = 32'd676;
        282: get_igmb = 32'd11380;
        283: get_igmb = 32'd7379;
        284: get_igmb = 32'd6501;
        285: get_igmb = 32'd4981;
        286: get_igmb = 32'd2628;
        287: get_igmb = 32'd3488;
        288: get_igmb = 32'd10956;
        289: get_igmb = 32'd7022;
        290: get_igmb = 32'd6737;
        291: get_igmb = 32'd9933;
        292: get_igmb = 32'd7139;
        293: get_igmb = 32'd2330;
        294: get_igmb = 32'd3884;
        295: get_igmb = 32'd5473;
        296: get_igmb = 32'd7865;
        297: get_igmb = 32'd6941;
        298: get_igmb = 32'd5737;
        299: get_igmb = 32'd5613;
        300: get_igmb = 32'd9505;
        301: get_igmb = 32'd11568;
        302: get_igmb = 32'd11277;
        303: get_igmb = 32'd2510;
        304: get_igmb = 32'd6689;
        305: get_igmb = 32'd386;
        306: get_igmb = 32'd4462;
        307: get_igmb = 32'd105;
        308: get_igmb = 32'd2076;
        309: get_igmb = 32'd10443;
        310: get_igmb = 32'd119;
        311: get_igmb = 32'd3955;
        312: get_igmb = 32'd4370;
        313: get_igmb = 32'd11505;
        314: get_igmb = 32'd3672;
        315: get_igmb = 32'd11439;
        316: get_igmb = 32'd750;
        317: get_igmb = 32'd3240;
        318: get_igmb = 32'd3133;
        319: get_igmb = 32'd754;
        320: get_igmb = 32'd4013;
        321: get_igmb = 32'd11929;
        322: get_igmb = 32'd9210;
        323: get_igmb = 32'd5378;
        324: get_igmb = 32'd11881;
        325: get_igmb = 32'd11018;
        326: get_igmb = 32'd2818;
        327: get_igmb = 32'd1851;
        328: get_igmb = 32'd4966;
        329: get_igmb = 32'd8181;
        330: get_igmb = 32'd2688;
        331: get_igmb = 32'd6205;
        332: get_igmb = 32'd6814;
        333: get_igmb = 32'd926;
        334: get_igmb = 32'd2936;
        335: get_igmb = 32'd4327;
        336: get_igmb = 32'd10175;
        337: get_igmb = 32'd7089;
        338: get_igmb = 32'd6047;
        339: get_igmb = 32'd9410;
        340: get_igmb = 32'd10492;
        341: get_igmb = 32'd8950;
        342: get_igmb = 32'd2472;
        343: get_igmb = 32'd6255;
        344: get_igmb = 32'd728;
        345: get_igmb = 32'd7569;
        346: get_igmb = 32'd6056;
        347: get_igmb = 32'd10432;
        348: get_igmb = 32'd11036;
        349: get_igmb = 32'd2452;
        350: get_igmb = 32'd2811;
        351: get_igmb = 32'd3787;
        352: get_igmb = 32'd945;
        353: get_igmb = 32'd8998;
        354: get_igmb = 32'd1244;
        355: get_igmb = 32'd8815;
        356: get_igmb = 32'd11017;
        357: get_igmb = 32'd11218;
        358: get_igmb = 32'd5894;
        359: get_igmb = 32'd4325;
        360: get_igmb = 32'd4639;
        361: get_igmb = 32'd3819;
        362: get_igmb = 32'd9826;
        363: get_igmb = 32'd7056;
        364: get_igmb = 32'd6786;
        365: get_igmb = 32'd8670;
        366: get_igmb = 32'd5539;
        367: get_igmb = 32'd7707;
        368: get_igmb = 32'd1361;
        369: get_igmb = 32'd9812;
        370: get_igmb = 32'd2949;
        371: get_igmb = 32'd11265;
        372: get_igmb = 32'd10301;
        373: get_igmb = 32'd9108;
        374: get_igmb = 32'd478;
        375: get_igmb = 32'd6489;
        376: get_igmb = 32'd101;
        377: get_igmb = 32'd1911;
        378: get_igmb = 32'd9483;
        379: get_igmb = 32'd3608;
        380: get_igmb = 32'd11997;
        381: get_igmb = 32'd10536;
        382: get_igmb = 32'd812;
        383: get_igmb = 32'd8915;
        384: get_igmb = 32'd637;
        385: get_igmb = 32'd8159;
        386: get_igmb = 32'd5299;
        387: get_igmb = 32'd9128;
        388: get_igmb = 32'd3512;
        389: get_igmb = 32'd8290;
        390: get_igmb = 32'd7068;
        391: get_igmb = 32'd7922;
        392: get_igmb = 32'd3036;
        393: get_igmb = 32'd4759;
        394: get_igmb = 32'd2163;
        395: get_igmb = 32'd3937;
        396: get_igmb = 32'd3755;
        397: get_igmb = 32'd11306;
        398: get_igmb = 32'd7739;
        399: get_igmb = 32'd4922;
        400: get_igmb = 32'd11932;
        401: get_igmb = 32'd424;
        402: get_igmb = 32'd5538;
        403: get_igmb = 32'd6228;
        404: get_igmb = 32'd11131;
        405: get_igmb = 32'd7778;
        406: get_igmb = 32'd11974;
        407: get_igmb = 32'd1097;
        408: get_igmb = 32'd2890;
        409: get_igmb = 32'd10027;
        410: get_igmb = 32'd2569;
        411: get_igmb = 32'd2250;
        412: get_igmb = 32'd2352;
        413: get_igmb = 32'd821;
        414: get_igmb = 32'd2550;
        415: get_igmb = 32'd11016;
        416: get_igmb = 32'd7769;
        417: get_igmb = 32'd136;
        418: get_igmb = 32'd617;
        419: get_igmb = 32'd3157;
        420: get_igmb = 32'd5889;
        421: get_igmb = 32'd9219;
        422: get_igmb = 32'd6855;
        423: get_igmb = 32'd120;
        424: get_igmb = 32'd4405;
        425: get_igmb = 32'd1825;
        426: get_igmb = 32'd9635;
        427: get_igmb = 32'd7214;
        428: get_igmb = 32'd10261;
        429: get_igmb = 32'd11393;
        430: get_igmb = 32'd2441;
        431: get_igmb = 32'd9562;
        432: get_igmb = 32'd11176;
        433: get_igmb = 32'd599;
        434: get_igmb = 32'd2085;
        435: get_igmb = 32'd11465;
        436: get_igmb = 32'd7233;
        437: get_igmb = 32'd6177;
        438: get_igmb = 32'd4801;
        439: get_igmb = 32'd9926;
        440: get_igmb = 32'd9010;
        441: get_igmb = 32'd4514;
        442: get_igmb = 32'd9455;
        443: get_igmb = 32'd11352;
        444: get_igmb = 32'd11670;
        445: get_igmb = 32'd6174;
        446: get_igmb = 32'd7950;
        447: get_igmb = 32'd9766;
        448: get_igmb = 32'd6896;
        449: get_igmb = 32'd11603;
        450: get_igmb = 32'd3213;
        451: get_igmb = 32'd8473;
        452: get_igmb = 32'd9873;
        453: get_igmb = 32'd2835;
        454: get_igmb = 32'd10422;
        455: get_igmb = 32'd3732;
        456: get_igmb = 32'd7961;
        457: get_igmb = 32'd1457;
        458: get_igmb = 32'd10857;
        459: get_igmb = 32'd8069;
        460: get_igmb = 32'd832;
        461: get_igmb = 32'd1628;
        462: get_igmb = 32'd3410;
        463: get_igmb = 32'd4900;
        464: get_igmb = 32'd10855;
        465: get_igmb = 32'd5111;
        466: get_igmb = 32'd9543;
        467: get_igmb = 32'd6325;
        468: get_igmb = 32'd7431;
        469: get_igmb = 32'd4083;
        470: get_igmb = 32'd3072;
        471: get_igmb = 32'd8847;
        472: get_igmb = 32'd9853;
        473: get_igmb = 32'd10122;
        474: get_igmb = 32'd5259;
        475: get_igmb = 32'd11413;
        476: get_igmb = 32'd6556;
        477: get_igmb = 32'd303;
        478: get_igmb = 32'd1465;
        479: get_igmb = 32'd3871;
        480: get_igmb = 32'd4873;
        481: get_igmb = 32'd5813;
        482: get_igmb = 32'd10017;
        483: get_igmb = 32'd6898;
        484: get_igmb = 32'd3311;
        485: get_igmb = 32'd5947;
        486: get_igmb = 32'd8637;
        487: get_igmb = 32'd5852;
        488: get_igmb = 32'd3856;
        489: get_igmb = 32'd928;
        490: get_igmb = 32'd4933;
        491: get_igmb = 32'd8530;
        492: get_igmb = 32'd1871;
        493: get_igmb = 32'd2184;
        494: get_igmb = 32'd5571;
        495: get_igmb = 32'd5879;
        496: get_igmb = 32'd3481;
        497: get_igmb = 32'd11597;
        498: get_igmb = 32'd9511;
        499: get_igmb = 32'd8153;
        500: get_igmb = 32'd35;
        501: get_igmb = 32'd2609;
        502: get_igmb = 32'd5963;
        503: get_igmb = 32'd8064;
        504: get_igmb = 32'd1080;
        505: get_igmb = 32'd12039;
        506: get_igmb = 32'd8444;
        507: get_igmb = 32'd3052;
        508: get_igmb = 32'd3813;
        509: get_igmb = 32'd11065;
        510: get_igmb = 32'd6736;
        511: get_igmb = 32'd8454;
        512: get_igmb = 32'd2340;
        513: get_igmb = 32'd7651;
        514: get_igmb = 32'd1910;
        515: get_igmb = 32'd10709;
        516: get_igmb = 32'd2117;
        517: get_igmb = 32'd9637;
        518: get_igmb = 32'd6402;
        519: get_igmb = 32'd6028;
        520: get_igmb = 32'd2124;
        521: get_igmb = 32'd7701;
        522: get_igmb = 32'd2679;
        523: get_igmb = 32'd5183;
        524: get_igmb = 32'd6270;
        525: get_igmb = 32'd7424;
        526: get_igmb = 32'd2597;
        527: get_igmb = 32'd6795;
        528: get_igmb = 32'd9222;
        529: get_igmb = 32'd10837;
        530: get_igmb = 32'd280;
        531: get_igmb = 32'd8583;
        532: get_igmb = 32'd3270;
        533: get_igmb = 32'd6753;
        534: get_igmb = 32'd2354;
        535: get_igmb = 32'd3779;
        536: get_igmb = 32'd6102;
        537: get_igmb = 32'd4732;
        538: get_igmb = 32'd5926;
        539: get_igmb = 32'd2497;
        540: get_igmb = 32'd8640;
        541: get_igmb = 32'd10289;
        542: get_igmb = 32'd6107;
        543: get_igmb = 32'd12127;
        544: get_igmb = 32'd2958;
        545: get_igmb = 32'd12287;
        546: get_igmb = 32'd10292;
        547: get_igmb = 32'd8086;
        548: get_igmb = 32'd817;
        549: get_igmb = 32'd4021;
        550: get_igmb = 32'd2610;
        551: get_igmb = 32'd1444;
        552: get_igmb = 32'd5899;
        553: get_igmb = 32'd11720;
        554: get_igmb = 32'd3292;
        555: get_igmb = 32'd2424;
        556: get_igmb = 32'd5090;
        557: get_igmb = 32'd7242;
        558: get_igmb = 32'd5205;
        559: get_igmb = 32'd5281;
        560: get_igmb = 32'd9956;
        561: get_igmb = 32'd2702;
        562: get_igmb = 32'd6656;
        563: get_igmb = 32'd735;
        564: get_igmb = 32'd2243;
        565: get_igmb = 32'd11656;
        566: get_igmb = 32'd833;
        567: get_igmb = 32'd3107;
        568: get_igmb = 32'd6012;
        569: get_igmb = 32'd6801;
        570: get_igmb = 32'd1126;
        571: get_igmb = 32'd6339;
        572: get_igmb = 32'd5250;
        573: get_igmb = 32'd10391;
        574: get_igmb = 32'd9642;
        575: get_igmb = 32'd5278;
        576: get_igmb = 32'd3513;
        577: get_igmb = 32'd9769;
        578: get_igmb = 32'd3025;
        579: get_igmb = 32'd779;
        580: get_igmb = 32'd9433;
        581: get_igmb = 32'd3392;
        582: get_igmb = 32'd7437;
        583: get_igmb = 32'd668;
        584: get_igmb = 32'd10184;
        585: get_igmb = 32'd8111;
        586: get_igmb = 32'd6527;
        587: get_igmb = 32'd6568;
        588: get_igmb = 32'd10831;
        589: get_igmb = 32'd6482;
        590: get_igmb = 32'd8263;
        591: get_igmb = 32'd5711;
        592: get_igmb = 32'd9780;
        593: get_igmb = 32'd467;
        594: get_igmb = 32'd5462;
        595: get_igmb = 32'd4425;
        596: get_igmb = 32'd11999;
        597: get_igmb = 32'd1205;
        598: get_igmb = 32'd5015;
        599: get_igmb = 32'd6918;
        600: get_igmb = 32'd5096;
        601: get_igmb = 32'd3827;
        602: get_igmb = 32'd5525;
        603: get_igmb = 32'd11579;
        604: get_igmb = 32'd3518;
        605: get_igmb = 32'd4875;
        606: get_igmb = 32'd7388;
        607: get_igmb = 32'd1931;
        608: get_igmb = 32'd6615;
        609: get_igmb = 32'd1541;
        610: get_igmb = 32'd8708;
        611: get_igmb = 32'd260;
        612: get_igmb = 32'd3385;
        613: get_igmb = 32'd4792;
        614: get_igmb = 32'd4391;
        615: get_igmb = 32'd5697;
        616: get_igmb = 32'd7895;
        617: get_igmb = 32'd2155;
        618: get_igmb = 32'd7337;
        619: get_igmb = 32'd236;
        620: get_igmb = 32'd10635;
        621: get_igmb = 32'd11534;
        622: get_igmb = 32'd1906;
        623: get_igmb = 32'd4793;
        624: get_igmb = 32'd9527;
        625: get_igmb = 32'd7239;
        626: get_igmb = 32'd8354;
        627: get_igmb = 32'd5121;
        628: get_igmb = 32'd10662;
        629: get_igmb = 32'd2311;
        630: get_igmb = 32'd3346;
        631: get_igmb = 32'd8556;
        632: get_igmb = 32'd707;
        633: get_igmb = 32'd1088;
        634: get_igmb = 32'd4936;
        635: get_igmb = 32'd678;
        636: get_igmb = 32'd10245;
        637: get_igmb = 32'd18;
        638: get_igmb = 32'd5684;
        639: get_igmb = 32'd960;
        640: get_igmb = 32'd4459;
        641: get_igmb = 32'd7957;
        642: get_igmb = 32'd226;
        643: get_igmb = 32'd2451;
        644: get_igmb = 32'd6;
        645: get_igmb = 32'd8874;
        646: get_igmb = 32'd320;
        647: get_igmb = 32'd6298;
        648: get_igmb = 32'd8963;
        649: get_igmb = 32'd8735;
        650: get_igmb = 32'd2852;
        651: get_igmb = 32'd2981;
        652: get_igmb = 32'd1707;
        653: get_igmb = 32'd5408;
        654: get_igmb = 32'd5017;
        655: get_igmb = 32'd9876;
        656: get_igmb = 32'd9790;
        657: get_igmb = 32'd2968;
        658: get_igmb = 32'd1899;
        659: get_igmb = 32'd6729;
        660: get_igmb = 32'd4183;
        661: get_igmb = 32'd5290;
        662: get_igmb = 32'd10084;
        663: get_igmb = 32'd7679;
        664: get_igmb = 32'd7941;
        665: get_igmb = 32'd8744;
        666: get_igmb = 32'd5694;
        667: get_igmb = 32'd3461;
        668: get_igmb = 32'd4175;
        669: get_igmb = 32'd5747;
        670: get_igmb = 32'd5561;
        671: get_igmb = 32'd3378;
        672: get_igmb = 32'd5227;
        673: get_igmb = 32'd952;
        674: get_igmb = 32'd4319;
        675: get_igmb = 32'd9810;
        676: get_igmb = 32'd4356;
        677: get_igmb = 32'd3088;
        678: get_igmb = 32'd11118;
        679: get_igmb = 32'd840;
        680: get_igmb = 32'd6257;
        681: get_igmb = 32'd486;
        682: get_igmb = 32'd6000;
        683: get_igmb = 32'd1342;
        684: get_igmb = 32'd10382;
        685: get_igmb = 32'd6017;
        686: get_igmb = 32'd4798;
        687: get_igmb = 32'd5489;
        688: get_igmb = 32'd4498;
        689: get_igmb = 32'd4193;
        690: get_igmb = 32'd2306;
        691: get_igmb = 32'd6521;
        692: get_igmb = 32'd1475;
        693: get_igmb = 32'd6372;
        694: get_igmb = 32'd9029;
        695: get_igmb = 32'd8037;
        696: get_igmb = 32'd1625;
        697: get_igmb = 32'd7020;
        698: get_igmb = 32'd4740;
        699: get_igmb = 32'd5730;
        700: get_igmb = 32'd7956;
        701: get_igmb = 32'd6351;
        702: get_igmb = 32'd6494;
        703: get_igmb = 32'd6917;
        704: get_igmb = 32'd11405;
        705: get_igmb = 32'd7487;
        706: get_igmb = 32'd10202;
        707: get_igmb = 32'd10155;
        708: get_igmb = 32'd7666;
        709: get_igmb = 32'd7556;
        710: get_igmb = 32'd11509;
        711: get_igmb = 32'd1546;
        712: get_igmb = 32'd6571;
        713: get_igmb = 32'd10199;
        714: get_igmb = 32'd2265;
        715: get_igmb = 32'd7327;
        716: get_igmb = 32'd5824;
        717: get_igmb = 32'd11396;
        718: get_igmb = 32'd11581;
        719: get_igmb = 32'd9722;
        720: get_igmb = 32'd2251;
        721: get_igmb = 32'd11199;
        722: get_igmb = 32'd5356;
        723: get_igmb = 32'd7408;
        724: get_igmb = 32'd2861;
        725: get_igmb = 32'd4003;
        726: get_igmb = 32'd9215;
        727: get_igmb = 32'd484;
        728: get_igmb = 32'd7526;
        729: get_igmb = 32'd9409;
        730: get_igmb = 32'd12235;
        731: get_igmb = 32'd6157;
        732: get_igmb = 32'd9025;
        733: get_igmb = 32'd2121;
        734: get_igmb = 32'd10255;
        735: get_igmb = 32'd2519;
        736: get_igmb = 32'd9533;
        737: get_igmb = 32'd3824;
        738: get_igmb = 32'd8674;
        739: get_igmb = 32'd11419;
        740: get_igmb = 32'd10888;
        741: get_igmb = 32'd4762;
        742: get_igmb = 32'd11303;
        743: get_igmb = 32'd4097;
        744: get_igmb = 32'd2414;
        745: get_igmb = 32'd6496;
        746: get_igmb = 32'd9953;
        747: get_igmb = 32'd10554;
        748: get_igmb = 32'd808;
        749: get_igmb = 32'd2999;
        750: get_igmb = 32'd2130;
        751: get_igmb = 32'd4286;
        752: get_igmb = 32'd12078;
        753: get_igmb = 32'd7445;
        754: get_igmb = 32'd5132;
        755: get_igmb = 32'd7915;
        756: get_igmb = 32'd245;
        757: get_igmb = 32'd5974;
        758: get_igmb = 32'd4874;
        759: get_igmb = 32'd7292;
        760: get_igmb = 32'd7560;
        761: get_igmb = 32'd10539;
        762: get_igmb = 32'd9952;
        763: get_igmb = 32'd9075;
        764: get_igmb = 32'd2113;
        765: get_igmb = 32'd3721;
        766: get_igmb = 32'd10285;
        767: get_igmb = 32'd10022;
        768: get_igmb = 32'd9578;
        769: get_igmb = 32'd8934;
        770: get_igmb = 32'd11074;
        771: get_igmb = 32'd9498;
        772: get_igmb = 32'd294;
        773: get_igmb = 32'd4711;
        774: get_igmb = 32'd3391;
        775: get_igmb = 32'd1377;
        776: get_igmb = 32'd9072;
        777: get_igmb = 32'd10189;
        778: get_igmb = 32'd4569;
        779: get_igmb = 32'd10890;
        780: get_igmb = 32'd9909;
        781: get_igmb = 32'd6923;
        782: get_igmb = 32'd53;
        783: get_igmb = 32'd4653;
        784: get_igmb = 32'd439;
        785: get_igmb = 32'd10253;
        786: get_igmb = 32'd7028;
        787: get_igmb = 32'd10207;
        788: get_igmb = 32'd8343;
        789: get_igmb = 32'd1141;
        790: get_igmb = 32'd2556;
        791: get_igmb = 32'd7601;
        792: get_igmb = 32'd8150;
        793: get_igmb = 32'd10630;
        794: get_igmb = 32'd8648;
        795: get_igmb = 32'd9832;
        796: get_igmb = 32'd7951;
        797: get_igmb = 32'd11245;
        798: get_igmb = 32'd2131;
        799: get_igmb = 32'd5765;
        800: get_igmb = 32'd10343;
        801: get_igmb = 32'd9781;
        802: get_igmb = 32'd2718;
        803: get_igmb = 32'd1419;
        804: get_igmb = 32'd4531;
        805: get_igmb = 32'd3844;
        806: get_igmb = 32'd4066;
        807: get_igmb = 32'd4293;
        808: get_igmb = 32'd11657;
        809: get_igmb = 32'd11525;
        810: get_igmb = 32'd11353;
        811: get_igmb = 32'd4313;
        812: get_igmb = 32'd4869;
        813: get_igmb = 32'd12186;
        814: get_igmb = 32'd1611;
        815: get_igmb = 32'd10892;
        816: get_igmb = 32'd11489;
        817: get_igmb = 32'd8833;
        818: get_igmb = 32'd2393;
        819: get_igmb = 32'd15;
        820: get_igmb = 32'd10830;
        821: get_igmb = 32'd5003;
        822: get_igmb = 32'd17;
        823: get_igmb = 32'd565;
        824: get_igmb = 32'd5891;
        825: get_igmb = 32'd12177;
        826: get_igmb = 32'd11058;
        827: get_igmb = 32'd10412;
        828: get_igmb = 32'd8885;
        829: get_igmb = 32'd3974;
        830: get_igmb = 32'd10981;
        831: get_igmb = 32'd7130;
        832: get_igmb = 32'd5840;
        833: get_igmb = 32'd10482;
        834: get_igmb = 32'd8338;
        835: get_igmb = 32'd6035;
        836: get_igmb = 32'd6964;
        837: get_igmb = 32'd1574;
        838: get_igmb = 32'd10936;
        839: get_igmb = 32'd2020;
        840: get_igmb = 32'd2465;
        841: get_igmb = 32'd8191;
        842: get_igmb = 32'd384;
        843: get_igmb = 32'd2642;
        844: get_igmb = 32'd2729;
        845: get_igmb = 32'd5399;
        846: get_igmb = 32'd2175;
        847: get_igmb = 32'd9396;
        848: get_igmb = 32'd11987;
        849: get_igmb = 32'd8035;
        850: get_igmb = 32'd4375;
        851: get_igmb = 32'd6611;
        852: get_igmb = 32'd5010;
        853: get_igmb = 32'd11812;
        854: get_igmb = 32'd9131;
        855: get_igmb = 32'd11427;
        856: get_igmb = 32'd104;
        857: get_igmb = 32'd6348;
        858: get_igmb = 32'd9643;
        859: get_igmb = 32'd6757;
        860: get_igmb = 32'd12110;
        861: get_igmb = 32'd5617;
        862: get_igmb = 32'd10935;
        863: get_igmb = 32'd541;
        864: get_igmb = 32'd135;
        865: get_igmb = 32'd3041;
        866: get_igmb = 32'd7200;
        867: get_igmb = 32'd6526;
        868: get_igmb = 32'd5085;
        869: get_igmb = 32'd12136;
        870: get_igmb = 32'd842;
        871: get_igmb = 32'd4129;
        872: get_igmb = 32'd7685;
        873: get_igmb = 32'd11079;
        874: get_igmb = 32'd8426;
        875: get_igmb = 32'd1008;
        876: get_igmb = 32'd2725;
        877: get_igmb = 32'd11772;
        878: get_igmb = 32'd6058;
        879: get_igmb = 32'd1101;
        880: get_igmb = 32'd1950;
        881: get_igmb = 32'd8424;
        882: get_igmb = 32'd5688;
        883: get_igmb = 32'd6876;
        884: get_igmb = 32'd12005;
        885: get_igmb = 32'd10079;
        886: get_igmb = 32'd5335;
        887: get_igmb = 32'd927;
        888: get_igmb = 32'd1770;
        889: get_igmb = 32'd273;
        890: get_igmb = 32'd8377;
        891: get_igmb = 32'd2271;
        892: get_igmb = 32'd5225;
        893: get_igmb = 32'd10283;
        894: get_igmb = 32'd116;
        895: get_igmb = 32'd11807;
        896: get_igmb = 32'd91;
        897: get_igmb = 32'd11699;
        898: get_igmb = 32'd757;
        899: get_igmb = 32'd1304;
        900: get_igmb = 32'd7524;
        901: get_igmb = 32'd6451;
        902: get_igmb = 32'd8032;
        903: get_igmb = 32'd8154;
        904: get_igmb = 32'd7456;
        905: get_igmb = 32'd4191;
        906: get_igmb = 32'd309;
        907: get_igmb = 32'd2318;
        908: get_igmb = 32'd2292;
        909: get_igmb = 32'd10393;
        910: get_igmb = 32'd11639;
        911: get_igmb = 32'd9481;
        912: get_igmb = 32'd12238;
        913: get_igmb = 32'd10594;
        914: get_igmb = 32'd9569;
        915: get_igmb = 32'd7912;
        916: get_igmb = 32'd10368;
        917: get_igmb = 32'd9889;
        918: get_igmb = 32'd12244;
        919: get_igmb = 32'd7179;
        920: get_igmb = 32'd3924;
        921: get_igmb = 32'd3188;
        922: get_igmb = 32'd367;
        923: get_igmb = 32'd2077;
        924: get_igmb = 32'd336;
        925: get_igmb = 32'd5384;
        926: get_igmb = 32'd5631;
        927: get_igmb = 32'd8596;
        928: get_igmb = 32'd4621;
        929: get_igmb = 32'd1775;
        930: get_igmb = 32'd8866;
        931: get_igmb = 32'd451;
        932: get_igmb = 32'd6108;
        933: get_igmb = 32'd1317;
        934: get_igmb = 32'd6246;
        935: get_igmb = 32'd8795;
        936: get_igmb = 32'd5896;
        937: get_igmb = 32'd7283;
        938: get_igmb = 32'd3132;
        939: get_igmb = 32'd11564;
        940: get_igmb = 32'd4977;
        941: get_igmb = 32'd12161;
        942: get_igmb = 32'd7371;
        943: get_igmb = 32'd1366;
        944: get_igmb = 32'd12130;
        945: get_igmb = 32'd10619;
        946: get_igmb = 32'd3809;
        947: get_igmb = 32'd5149;
        948: get_igmb = 32'd6300;
        949: get_igmb = 32'd2638;
        950: get_igmb = 32'd4197;
        951: get_igmb = 32'd1418;
        952: get_igmb = 32'd10065;
        953: get_igmb = 32'd4156;
        954: get_igmb = 32'd8373;
        955: get_igmb = 32'd8644;
        956: get_igmb = 32'd10445;
        957: get_igmb = 32'd882;
        958: get_igmb = 32'd8158;
        959: get_igmb = 32'd10173;
        960: get_igmb = 32'd9763;
        961: get_igmb = 32'd12191;
        962: get_igmb = 32'd459;
        963: get_igmb = 32'd2966;
        964: get_igmb = 32'd3166;
        965: get_igmb = 32'd405;
        966: get_igmb = 32'd5000;
        967: get_igmb = 32'd9311;
        968: get_igmb = 32'd6404;
        969: get_igmb = 32'd8986;
        970: get_igmb = 32'd1551;
        971: get_igmb = 32'd8175;
        972: get_igmb = 32'd3630;
        973: get_igmb = 32'd10766;
        974: get_igmb = 32'd9265;
        975: get_igmb = 32'd700;
        976: get_igmb = 32'd8573;
        977: get_igmb = 32'd9508;
        978: get_igmb = 32'd6630;
        979: get_igmb = 32'd11437;
        980: get_igmb = 32'd11595;
        981: get_igmb = 32'd5850;
        982: get_igmb = 32'd3950;
        983: get_igmb = 32'd4775;
        984: get_igmb = 32'd11941;
        985: get_igmb = 32'd1446;
        986: get_igmb = 32'd6018;
        987: get_igmb = 32'd3386;
        988: get_igmb = 32'd11470;
        989: get_igmb = 32'd5310;
        990: get_igmb = 32'd5476;
        991: get_igmb = 32'd553;
        992: get_igmb = 32'd9474;
        993: get_igmb = 32'd2586;
        994: get_igmb = 32'd1431;
        995: get_igmb = 32'd2741;
        996: get_igmb = 32'd473;
        997: get_igmb = 32'd11383;
        998: get_igmb = 32'd4745;
        999: get_igmb = 32'd836;
        1000: get_igmb = 32'd4062;
        1001: get_igmb = 32'd10666;
        1002: get_igmb = 32'd7727;
        1003: get_igmb = 32'd11752;
        1004: get_igmb = 32'd5534;
        1005: get_igmb = 32'd312;
        1006: get_igmb = 32'd4307;
        1007: get_igmb = 32'd4351;
        1008: get_igmb = 32'd5764;
        1009: get_igmb = 32'd8679;
        1010: get_igmb = 32'd8381;
        1011: get_igmb = 32'd8187;
        1012: get_igmb = 32'd5;
        1013: get_igmb = 32'd7395;
        1014: get_igmb = 32'd4363;
        1015: get_igmb = 32'd1152;
        1016: get_igmb = 32'd5421;
        1017: get_igmb = 32'd5231;
        1018: get_igmb = 32'd6473;
        1019: get_igmb = 32'd436;
        1020: get_igmb = 32'd7567;
        1021: get_igmb = 32'd8603;
        1022: get_igmb = 32'd6229;
        1023: get_igmb = 32'd8230;
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
        x[i] = montgomery_mul(x[i], NI1024);
      end
    end
  endtask

  task automatic intt1024_compute(inout logic [31:0] x [0:N-1]);
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
        out_count_d    = 10'd0;

        if (fifo_req_i.push && !fifo_resp_o.full) begin
          a_d[in_count_q] = fifo_req_i.data;

          if (in_count_q == 10'd1023) begin
            in_count_d = 10'd0;
            state_d    = S_COMPUTE;
          end else begin
            in_count_d = in_count_q + 10'd1;
          end
        end
      end

      S_COMPUTE: begin
        intt1024_compute(a_d);

        fifo_data_d    = a_d[0];
        output_valid_d = 1'b1;
        out_count_d    = 10'd0;
        state_d        = S_OUTPUT;
      end

      S_OUTPUT: begin
        fifo_data_d    = a_q[out_count_q];
        output_valid_d = 1'b1;

        if (fifo_req_i.pop && output_valid_q) begin
          if (out_count_q == 10'd1023) begin
            out_count_d    = 10'd0;
            output_valid_d = 1'b0;
            state_d        = S_INPUT;
          end else begin
            out_count_d = out_count_q + 10'd1;
            fifo_data_d = a_q[out_count_q + 10'd1];
          end
        end
      end

      default: begin
        state_d = S_INPUT;
      end

    endcase

    if (fifo_req_i.flush) begin
      state_d        = S_INPUT;
      in_count_d     = 10'd0;
      out_count_d    = 10'd0;
      output_valid_d = 1'b0;
      fifo_data_d    = 32'd0;
    end
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      state_q        <= S_INPUT;
      in_count_q     <= 10'd0;
      out_count_q    <= 10'd0;
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
