module falcon_accel (
    input  logic clk_i,
    input  logic rst_ni,

    input  reg_pkg::reg_req_t reg_req_i,
    output reg_pkg::reg_rsp_t reg_rsp_o
);

  localparam logic [11:0] CTRL_OFFSET     = 12'h000;
  localparam logic [11:0] STATUS_OFFSET   = 12'h004;
  localparam logic [11:0] DATA_IN_OFFSET  = 12'h008;
  localparam logic [11:0] DATA_OUT_OFFSET = 12'h00C;

  logic [31:0] data_in_q;
  logic [31:0] data_out_q;
  logic        done_q;

  logic [31:0] rdata_d;
  logic        unused_wstrb;

  assign unused_wstrb = ^reg_req_i.wstrb;

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      data_in_q  <= 32'h0;
      data_out_q <= 32'h0;
      done_q     <= 1'b0;
    end else begin
      if (reg_req_i.valid && reg_req_i.write) begin
        unique case (reg_req_i.addr[11:0])
          CTRL_OFFSET: begin
            if (reg_req_i.wdata[0]) begin
              data_out_q <= data_in_q + 32'd1;
              done_q     <= 1'b1;
            end
            if (reg_req_i.wdata[1]) begin
              done_q <= 1'b0;
            end
          end

          DATA_IN_OFFSET: begin
            data_in_q <= reg_req_i.wdata;
            done_q    <= 1'b0;
          end

          default: begin
          end
        endcase
      end
    end
  end

  always_comb begin
    rdata_d = 32'h0;

    unique case (reg_req_i.addr[11:0])
      CTRL_OFFSET: begin
        rdata_d = 32'h0;
      end

      STATUS_OFFSET: begin
        rdata_d = {31'b0, done_q};
      end

      DATA_IN_OFFSET: begin
        rdata_d = data_in_q;
      end

      DATA_OUT_OFFSET: begin
        rdata_d = data_out_q;
      end

      default: begin
        rdata_d = 32'h0;
      end
    endcase
  end

  assign reg_rsp_o.ready = reg_req_i.valid;
  assign reg_rsp_o.error = 1'b0;
  assign reg_rsp_o.rdata = rdata_d;

endmodule
