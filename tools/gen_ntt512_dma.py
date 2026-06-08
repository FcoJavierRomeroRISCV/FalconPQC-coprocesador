#!/usr/bin/env python3
import re
from pathlib import Path

GMb_SOURCE = Path("external_tables/header.h")

N = 512

RTL_DIR = Path("hw/falcon_ntt512_dma_accel/rtl")
IP_DIR = Path("hw/falcon_ntt512_dma_accel")
APP_DIR = Path("sw/applications/falcon_ntt512_dma_test")

RTL_DIR.mkdir(parents=True, exist_ok=True)
IP_DIR.mkdir(parents=True, exist_ok=True)
APP_DIR.mkdir(parents=True, exist_ok=True)

if not GMb_SOURCE.exists():
    raise FileNotFoundError(
        f"No existe {GMb_SOURCE}. Primero copia header.h en external_tables/header.h"
    )

text = GMb_SOURCE.read_text()

m = re.search(
    r"static\s+const\s+uint16_t\s+GMb\s*\[\s*1024\s*\]\s*=\s*\{(?P<body>.*?)\};",
    text,
    re.S,
)

if not m:
    raise RuntimeError(f"No he encontrado GMb[1024] en {GMb_SOURCE}")

body = m.group("body")
nums = [int(x) for x in re.findall(r"\b\d+\b", body)]

if len(nums) < N:
    raise RuntimeError(f"GMb tiene solo {len(nums)} valores, necesito al menos {N}")

GMb = nums[:N]

print(f"GMb detectada con {len(nums)} valores. Usando GMb[0..{N - 1}].")

def gen_case_gmb():
    return "\n".join(
        f"        {i}: get_gmb = 32'd{GMb[i]};"
        for i in range(1, N)
    )

rtl = f"""module falcon_ntt512_dma_accel (
    input logic clk_i,
    input logic rst_ni,

    output logic fifo_req_done,
    input  dma_fifo_pkg::fifo_req_t  fifo_req_i,
    output dma_fifo_pkg::fifo_resp_t fifo_resp_o
);

  localparam int unsigned N = 512;

  localparam logic [31:0] Q   = 32'd12289;
  localparam logic [31:0] Q0I = 32'd12287;

  typedef enum logic [2:0] {{
    S_INPUT,
    S_COMPUTE,
    S_OUTPUT
  }} state_t;

  state_t state_q, state_d;

  logic [31:0] a_q [0:N-1];
  logic [31:0] a_d [0:N-1];

  logic [8:0] in_count_q, in_count_d;
  logic [8:0] out_count_q, out_count_d;

  logic output_valid_q, output_valid_d;

  logic [31:0] fifo_data_q;
  logic [31:0] fifo_data_d;

  function automatic logic [31:0] get_gmb(input int unsigned idx);
    begin
      unique case (idx)
{gen_case_gmb()}
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

  task automatic ntt512_compute(inout logic [31:0] x [0:N-1]);
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
        out_count_d    = 9'd0;

        if (fifo_req_i.push && !fifo_resp_o.full) begin
          a_d[in_count_q] = fifo_req_i.data;

          if (in_count_q == 9'd511) begin
            in_count_d = 9'd0;
            state_d    = S_COMPUTE;
          end else begin
            in_count_d = in_count_q + 9'd1;
          end
        end
      end

      S_COMPUTE: begin
        ntt512_compute(a_d);

        fifo_data_d    = a_d[0];
        output_valid_d = 1'b1;
        out_count_d    = 9'd0;
        state_d        = S_OUTPUT;
      end

      S_OUTPUT: begin
        fifo_data_d    = a_q[out_count_q];
        output_valid_d = 1'b1;

        if (fifo_req_i.pop && output_valid_q) begin
          if (out_count_q == 9'd511) begin
            out_count_d    = 9'd0;
            output_valid_d = 1'b0;
            state_d        = S_INPUT;
          end else begin
            out_count_d = out_count_q + 9'd1;
            fifo_data_d = a_q[out_count_q + 9'd1];
          end
        end
      end

      default: begin
        state_d = S_INPUT;
      end

    endcase

    if (fifo_req_i.flush) begin
      state_d        = S_INPUT;
      in_count_d     = 9'd0;
      out_count_d    = 9'd0;
      output_valid_d = 1'b0;
      fifo_data_d    = 32'd0;
    end
  end

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      state_q        <= S_INPUT;
      in_count_q     <= 9'd0;
      out_count_q    <= 9'd0;
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
"""

(RTL_DIR / "falcon_ntt512_dma_accel.sv").write_text(rtl)

(IP_DIR / "falcon_ntt512_dma_accel.core").write_text("""CAPI=2:

name: javier:ip:falcon_ntt512_dma_accel
description: Falcon NTT512 DMA HW FIFO accelerator.

filesets:
  rtl:
    files:
    - rtl/falcon_ntt512_dma_accel.sv
    - falcon_ntt512_dma_accel.vlt : {file_type: vlt}
    file_type: systemVerilogSource

targets:
  default:
    filesets:
    - rtl
    toplevel: falcon_ntt512_dma_accel
""")

(IP_DIR / "falcon_ntt512_dma_accel.vlt").write_text("""`verilator_config

lint_off -rule SYNCASYNCNET -file "*falcon_ntt512_dma_accel*"
lint_off -rule UNUSEDSIGNAL -file "*falcon_ntt512_dma_accel*"
lint_off -rule WIDTHTRUNC -file "*falcon_ntt512_dma_accel*"
""")

gmb_c = ",\n    ".join(
    ", ".join(f"{v}u" for v in GMb[i:i+8])
    for i in range(0, N, 8)
)

main_c = f"""#include <stdio.h>
#include <stdint.h>

#include "dma.h"
#include "hart.h"

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

#define NTT_SIZE 512u
#define CH_NTT   0u

static uint32_t input_data[NTT_SIZE];
static uint32_t output_data[NTT_SIZE];
static uint32_t ref_data[NTT_SIZE];

static const uint32_t GMb[512] = {{
    {gmb_c}
}};

static uint32_t mod_add_ref(uint32_t u, uint32_t v) {{
    uint32_t tmp = u + v;

    if (tmp >= FALCON_Q) {{
        tmp -= FALCON_Q;
    }}

    return tmp;
}}

static uint32_t mod_sub_ref(uint32_t u, uint32_t v) {{
    if (u >= v) {{
        return u - v;
    }}

    return u + FALCON_Q - v;
}}

static uint32_t montgomery_ref(uint32_t x, uint32_t y) {{
    uint32_t z = x * y;
    uint32_t w = ((z * FALCON_Q0I) & 0xFFFFu) * FALCON_Q;
    uint32_t t = (z + w) >> 16;

    if (t >= FALCON_Q) {{
        t -= FALCON_Q;
    }}

    return t;
}}

static void butterfly_ref(uint32_t *x0, uint32_t *x1, uint32_t s) {{
    uint32_t u = *x0;
    uint32_t v = montgomery_ref(*x1, s);

    *x0 = mod_add_ref(u, v);
    *x1 = mod_sub_ref(u, v);
}}

static void ntt512_ref(uint32_t a[NTT_SIZE]) {{
    uint32_t t = NTT_SIZE;

    for (uint32_t m = 1; m < NTT_SIZE; m <<= 1) {{
        uint32_t ht = t >> 1;

        for (uint32_t i = 0; i < m; i++) {{
            uint32_t j1 = i * t;
            uint32_t j2 = j1 + ht;
            uint32_t s = GMb[m + i];

            for (uint32_t j = j1; j < j2; j++) {{
                butterfly_ref(&a[j], &a[j + ht], s);
            }}
        }}

        t = ht;
    }}
}}

static void make_test_vector(uint32_t test_id) {{
    for (uint32_t i = 0; i < NTT_SIZE; i++) {{
        if (test_id == 0u) {{
            input_data[i] = i;
        }} else if (test_id == 1u) {{
            input_data[i] = ((i + 1u) * 100u) % FALCON_Q;
        }} else if (test_id == 2u) {{
            input_data[i] = GMb[i];
        }} else {{
            input_data[i] = (FALCON_Q - 1u - i) % FALCON_Q;
        }}

        output_data[i] = 0u;
        ref_data[i] = input_data[i];
    }}
}}

static void print_first_last(const char *name, uint32_t v[NTT_SIZE]) {{
    printf("%s first 8: [", name);

    for (uint32_t i = 0; i < 8u; i++) {{
        printf("%u", v[i]);

        if (i != 7u) {{
            printf(", ");
        }}
    }}

    printf("]\\n");

    printf("%s last 8:  [", name);

    for (uint32_t i = NTT_SIZE - 8u; i < NTT_SIZE; i++) {{
        printf("%u", v[i]);

        if (i != NTT_SIZE - 1u) {{
            printf(", ");
        }}
    }}

    printf("]\\n");
}}

static int run_dma_ntt512_test(uint32_t test_id) {{
    int pass = 1;

    make_test_vector(test_id);
    ntt512_ref(ref_data);

    static dma_target_t src_target;
    static dma_target_t dst_target;
    static dma_trans_t trans;

    src_target = (dma_target_t){{0}};
    dst_target = (dma_target_t){{0}};
    trans = (dma_trans_t){{0}};

    src_target.ptr = (uint8_t *)input_data;
    src_target.inc_d1_du = 1;
    src_target.type = DMA_DATA_TYPE_WORD;
    src_target.trig = DMA_TRIG_MEMORY;

    dst_target.ptr = (uint8_t *)output_data;
    dst_target.inc_d1_du = 1;
    dst_target.type = DMA_DATA_TYPE_WORD;
    dst_target.trig = DMA_TRIG_MEMORY;

    trans.src = &src_target;
    trans.dst = &dst_target;
    trans.size_d1_du = NTT_SIZE;
    trans.dim = DMA_DIM_CONF_1D;
    trans.src_type = DMA_DATA_TYPE_WORD;
    trans.dst_type = DMA_DATA_TYPE_WORD;
    trans.mode = DMA_TRANS_MODE_SINGLE;
    trans.end = DMA_TRANS_END_INTR_WAIT;
    trans.channel = CH_NTT;
    trans.hw_fifo_en = 1;

    dma_config_flags_t res = dma_validate_transaction(
        &trans,
        DMA_ENABLE_REALIGN,
        DMA_PERFORM_CHECKS_INTEGRITY
    );

    if (res & DMA_CONFIG_CRITICAL_ERROR) {{
        printf("DMA validation failed\\n");
        return 0;
    }}

    res = dma_load_transaction(&trans);

    if (res & DMA_CONFIG_CRITICAL_ERROR) {{
        printf("DMA load failed\\n");
        return 0;
    }}

    res = dma_launch(&trans);

    if (res & DMA_CONFIG_CRITICAL_ERROR) {{
        printf("DMA launch failed\\n");
        return 0;
    }}

    printf("Running NTT512 test %u\\n", test_id);
    print_first_last("Input ", input_data);
    print_first_last("HW out", output_data);
    print_first_last("REF   ", ref_data);

    for (uint32_t i = 0; i < NTT_SIZE; i++) {{
        if (output_data[i] != ref_data[i]) {{
            printf("Mismatch at index %u: hw=%u ref=%u\\n",
                   i,
                   output_data[i],
                   ref_data[i]);
            pass = 0;
        }}
    }}

    if (pass) {{
        printf("NTT512 DMA Test PASS\\n");
    }} else {{
        printf("NTT512 DMA Test FAIL\\n");
    }}

    return pass;
}}

int main(void) {{
    int pass = 1;

    printf("Falcon NTT512 DMA accelerator test\\n");

#ifdef DMA_HW_FIFO_MODE
    printf("DMA_HW_FIFO_MODE enabled\\n");
#else
    printf("DMA_HW_FIFO_MODE disabled\\n");
#endif

    dma_init(NULL);

    pass &= run_dma_ntt512_test(0u);
    pass &= run_dma_ntt512_test(1u);
    pass &= run_dma_ntt512_test(2u);
    pass &= run_dma_ntt512_test(3u);

    if (pass) {{
        printf("Falcon NTT512 DMA accelerator test PASS\\n");
        return 0;
    }} else {{
        printf("Falcon NTT512 DMA accelerator test FAIL\\n");
        return 1;
    }}
}}
"""

(APP_DIR / "main.cpp").write_text(main_c)

print("Generado:")
print(" - hw/falcon_ntt512_dma_accel/rtl/falcon_ntt512_dma_accel.sv")
print(" - hw/falcon_ntt512_dma_accel/falcon_ntt512_dma_accel.core")
print(" - hw/falcon_ntt512_dma_accel/falcon_ntt512_dma_accel.vlt")
print(" - sw/applications/falcon_ntt512_dma_test/main.cpp")
