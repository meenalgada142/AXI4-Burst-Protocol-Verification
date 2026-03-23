//============================================================
// Top-Level DUT Module (AXI4 Full Burst Write + Read)
//============================================================
module dut (
    //=======================
    // AXI Global Signals
    //=======================
    input  wire         ACLK,
    input  wire         ARESETn,

    //=======================
    // AXI4 Write Address Channel
    //=======================
    input  wire [3:0]   AWID,
    input  wire [31:0]  AWADDR,
    input  wire [7:0]   AWLEN,
    input  wire [2:0]   AWSIZE,
    input  wire [1:0]   AWBURST,
    input  wire         AWVALID,
    output wire         AWREADY,

    //=======================
    // AXI4 Write Data Channel
    //=======================
    input  wire [31:0]  WDATA,
    input  wire [3:0]   WSTRB,
    input  wire         WLAST,
    input  wire         WVALID,
    output wire         WREADY,

    //=======================
    // AXI4 Write Response Channel
    //=======================
    output wire [3:0]   BID,
    output wire [1:0]   BRESP,
    output wire         BVALID,
    input  wire         BREADY,

    //=======================
    // AXI4 Read Address Channel (upgraded)
    //=======================
    input  wire [3:0]   ARID,
    input  wire [31:0]  ARADDR,
    input  wire [7:0]   ARLEN,
    input  wire [2:0]   ARSIZE,
    input  wire [1:0]   ARBURST,
    input  wire         ARVALID,
    output wire         ARREADY,

    //=======================
    // AXI4 Read Data Channel (upgraded)
    //=======================
    output wire [3:0]   RID,
    output wire [31:0]  RDATA,
    output wire [1:0]   RRESP,
    output wire         RLAST,
    output wire         RVALID,
    input  wire         RREADY,

    //=======================
    // FIFO Status Output
    //=======================
    output wire [4:0]   fifo_level
);

    //============================================================
    // AXI4 Burst FIFO Wrapper Instantiation
    // Bug injection: all disabled (correct behavior)
    // Set BUG_RD_COUNTER_INIT=1, BUG_RLAST_EARLY=1, or
    // BUG_RD_ADDR_NO_HSHK=1 to inject read-channel bugs.
    //============================================================
    axi4_burst_fifo_wrapper #(
        .DATA_WIDTH          (32),
        .ADDR_WIDTH          (32),
        .ID_WIDTH            (4),
        .FIFO_DWIDTH         (8),
        .FIFO_DEPTH          (16),
        .MEM_DEPTH           (256),
        .BUG_RD_COUNTER_INIT (0),
        .BUG_RLAST_EARLY     (0),
        .BUG_RD_ADDR_NO_HSHK(0)
    ) u_axi4_fifo (
        .ACLK       (ACLK),
        .ARESETn    (ARESETn),

        // Write address channel
        .AWID       (AWID),
        .AWADDR     (AWADDR),
        .AWLEN      (AWLEN),
        .AWSIZE     (AWSIZE),
        .AWBURST    (AWBURST),
        .AWVALID    (AWVALID),
        .AWREADY    (AWREADY),

        // Write data channel
        .WDATA      (WDATA),
        .WSTRB      (WSTRB),
        .WLAST      (WLAST),
        .WVALID     (WVALID),
        .WREADY     (WREADY),

        // Write response channel
        .BID        (BID),
        .BRESP      (BRESP),
        .BVALID     (BVALID),
        .BREADY     (BREADY),

        // Read address channel (upgraded)
        .ARID       (ARID),
        .ARADDR     (ARADDR),
        .ARLEN      (ARLEN),
        .ARSIZE     (ARSIZE),
        .ARBURST    (ARBURST),
        .ARVALID    (ARVALID),
        .ARREADY    (ARREADY),

        // Read data channel (upgraded)
        .RID        (RID),
        .RDATA      (RDATA),
        .RRESP      (RRESP),
        .RLAST      (RLAST),
        .RVALID     (RVALID),
        .RREADY     (RREADY),

        // FIFO status
        .fifo_level (fifo_level)
    );

endmodule
