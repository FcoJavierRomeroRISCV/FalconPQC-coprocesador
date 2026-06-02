module falcon_dma_loopback_accel (
    input logic clk_i,
    input logic rst_ni,

    output logic fifo_req_done,
    input  dma_fifo_pkg::fifo_req_t  fifo_req_i,
    output dma_fifo_pkg::fifo_resp_t fifo_resp_o
);

  logic fifo_full;

  fifo_v3 #(
      .DEPTH(16),
      .FALL_THROUGH(1'b1),
      .DATA_WIDTH(32)
  ) passthrough_i (
      .clk_i(clk_i),
      .rst_ni(rst_ni),
      .flush_i(1'b0),
      .testmode_i(1'b0),
      .full_o(fifo_full),
      .empty_o(fifo_resp_o.empty),
      .usage_o(),
      .data_i(fifo_req_i.data),
      .push_i(fifo_req_i.push),
      .data_o(fifo_resp_o.data),
      .pop_i(fifo_req_i.pop)
  );

  assign fifo_resp_o.full     = fifo_full;
  assign fifo_resp_o.alm_full = fifo_full;

  /*
   * Important:
   * Do not force done=1. In HW FIFO mode, done=1 with empty=1
   * can make the DMA write FSM terminate before any data is written.
   * The DMA transaction should finish by its internal transfer counter.
   */
  assign fifo_req_done = 1'b0;

endmodule
