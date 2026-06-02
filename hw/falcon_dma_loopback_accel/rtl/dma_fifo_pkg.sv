package dma_fifo_pkg;

  typedef struct packed {
    logic pop;
    logic push;
    logic flush;
    logic [31:0] data;
  } fifo_req_t;

  typedef struct packed {
    logic empty;
    logic full;
    logic alm_full;
    logic [31:0] data;
  } fifo_resp_t;

endpackage
