//============================================================
// AXI4 Full Burst Write + Read Controller with FIFO Slave
//============================================================
//
// Write channel: UNCHANGED from previous version.
//
// Read channel: FIXED AXI4 READ BURST IMPLEMENTATION
//   Corrections applied (vs. previous version):
//
//   FIX 1: AR handshake — ARREADY driven unconditionally HIGH
//          in R_IDLE (no dependency on ARVALID). Latch occurs
//          ONLY on (ARVALID && ARREADY), not ARVALID alone.
//
//   FIX 2: FIFO/register region isolation — addresses < 0x100
//          force read_len = 0, preventing mixed FIFO+memory
//          burst behavior.
//
//   FIX 3: Unified RLAST — single canonical condition:
//          final_beat = (read_counter == read_len)
//          No duplicate definitions. BUG_RLAST_EARLY is a
//          clean modifier on the ONE canonical wire.
//
//   FIX 4: Registered data pipeline — first beat data computed
//          via rdata_next in R_IDLE, presented on RDATA one
//          cycle later in R_BURST. No combinational path from
//          ARADDR to RDATA.
//
//   FIX 5: Strict backpressure — when RVALID=1 and RREADY=0,
//          ALL R-channel outputs (RDATA, RLAST, RID, RRESP)
//          and ALL internal state (read_counter, read_addr)
//          are FROZEN. Zero speculative updates.
//
//   FIX 6: Counter/address gating — increments ONLY inside
//          the (RVALID && RREADY) handshake guard.
//
//   FIX 7: RID assigned ONCE from read_id register. No
//          redundant assignments across states.
//
//   FIX 8: Clean FSM — RVALID asserted ONLY in R_BURST,
//          ARREADY asserted ONLY in R_IDLE, no overlap.
//
//   FIX 9: Separated datapath — rdata_next register stages
//          the next beat's data independently from control
//          logic. read_addr_next wire pre-computes address
//          without side effects.
//
// Bug injection parameters (WaveEye integration):
//   BUG_RD_COUNTER_INIT : 0=correct, 1=off-by-one counter
//   BUG_RLAST_EARLY     : 0=correct, 1=RLAST fires one beat early
//   BUG_RD_ADDR_NO_HSHK : 0=correct, 1=addr advances without RREADY
//============================================================

module axi4_burst_fifo_wrapper #(
    parameter DATA_WIDTH  = 32,
    parameter ADDR_WIDTH  = 32,
    parameter ID_WIDTH    = 4,
    parameter FIFO_DWIDTH = 8,
    parameter FIFO_DEPTH  = 16,
    parameter MEM_DEPTH   = 256,

    // ── Bug injection parameters (read channel) ──
    parameter BUG_RD_COUNTER_INIT = 0,
    parameter BUG_RLAST_EARLY     = 0,
    parameter BUG_RD_ADDR_NO_HSHK = 0
)(
    //=======================
    // AXI Global Signals
    //=======================
    input  wire                       ACLK,
    input  wire                       ARESETn,

    //=======================
    // AXI4 Write Address Channel
    //=======================
    input  wire [ID_WIDTH-1:0]        AWID,
    input  wire [ADDR_WIDTH-1:0]      AWADDR,
    input  wire [7:0]                 AWLEN,
    input  wire [2:0]                 AWSIZE,
    input  wire [1:0]                 AWBURST,
    input  wire                       AWVALID,
    output reg                        AWREADY,

    //=======================
    // AXI4 Write Data Channel
    //=======================
    input  wire [DATA_WIDTH-1:0]      WDATA,
    input  wire [(DATA_WIDTH/8)-1:0]  WSTRB,
    input  wire                       WLAST,
    input  wire                       WVALID,
    output reg                        WREADY,

    //=======================
    // AXI4 Write Response Channel
    //=======================
    output reg  [ID_WIDTH-1:0]        BID,
    output reg  [1:0]                 BRESP,
    output reg                        BVALID,
    input  wire                       BREADY,

    //=======================
    // AXI4 Read Address Channel
    //=======================
    input  wire [ID_WIDTH-1:0]        ARID,
    input  wire [ADDR_WIDTH-1:0]      ARADDR,
    input  wire [7:0]                 ARLEN,
    input  wire [2:0]                 ARSIZE,
    input  wire [1:0]                 ARBURST,
    input  wire                       ARVALID,
    output reg                        ARREADY,

    //=======================
    // AXI4 Read Data Channel
    //=======================
    output reg  [ID_WIDTH-1:0]        RID,
    output reg  [DATA_WIDTH-1:0]      RDATA,
    output reg  [1:0]                 RRESP,
    output reg                        RLAST,
    output reg                        RVALID,
    input  wire                       RREADY,

    //=======================
    // FIFO Status Output
    //=======================
    output wire [4:0]                 fifo_level
);

    // ============================================================
    // Local Parameters
    // ============================================================
    localparam STRB_WIDTH = DATA_WIDTH / 8;

    wire _unused = &{1'b0, ARADDR[ADDR_WIDTH-1:12]};

    // Write FSM states (unchanged)
    localparam [1:0] W_IDLE       = 2'b00,
                     W_WRITE_DATA = 2'b01,
                     W_WRITE_RESP = 2'b10;

    // Read FSM states
    //   R_IDLE  : ARREADY=1, RVALID=0. Waiting for handshake.
    //   R_BURST : ARREADY=0, RVALID=1. Delivering data beats.
    localparam [1:0] R_IDLE  = 2'b00,
                     R_BURST = 2'b01;

    // AXI burst types
    localparam [1:0] BURST_FIXED = 2'b00,
                     BURST_INCR  = 2'b01,
                     BURST_WRAP  = 2'b10;

    // AXI response codes
    localparam [1:0] RESP_OKAY   = 2'b00,
                     RESP_SLVERR = 2'b10;

    // ============================================================
    // Internal Memory Array (shared between read and write)
    // ============================================================
    reg [DATA_WIDTH-1:0] mem [0:MEM_DEPTH-1];

    // ============================================================
    // FIFO Instantiation (unchanged)
    // ============================================================
    reg  wr_en, rd_en;
    wire [FIFO_DWIDTH-1:0] fifo_dout;
    wire fifo_full, fifo_empty;
    wire [4:0] fifo_count;

    fifo #(
        .DATA_WIDTH (FIFO_DWIDTH),
        .DEPTH      (FIFO_DEPTH)
    ) u_fifo (
        .clk   (ACLK),
        .rst   (~ARESETn),
        .wr_en (wr_en),
        .rd_en (rd_en),
        .din   (WDATA[FIFO_DWIDTH-1:0]),
        .dout  (fifo_dout),
        .full  (fifo_full),
        .empty (fifo_empty),
        .count (fifo_count)
    );

    assign fifo_level = fifo_count;

    // ============================================================
    // Write Channel Registers (unchanged)
    // ============================================================
    reg [ADDR_WIDTH-1:0]  burst_addr;
    reg [7:0]             burst_len;
    reg [2:0]             burst_size;
    reg [1:0]             burst_type;
    reg [ID_WIDTH-1:0]    write_id;
    reg [7:0]             beat_counter;
    reg                   wlast_error;
    reg [1:0]             write_state;

    // ============================================================
    // Address Increment Logic (shared by read and write)
    // ============================================================
    function automatic [ADDR_WIDTH-1:0] next_burst_addr(
        input [ADDR_WIDTH-1:0] current_addr,
        input [2:0]            size,
        input [1:0]            btype,
        input [7:0]            len
    );
        reg [ADDR_WIDTH-1:0] incr;
        reg [ADDR_WIDTH-1:0] wrap_mask;
        reg [ADDR_WIDTH-1:0] next_addr;
    begin
        incr = (1 << size);
        case (btype)
            BURST_FIXED: next_addr = current_addr;
            BURST_INCR:  next_addr = current_addr + incr;
            BURST_WRAP: begin
                wrap_mask = (({24'd0, len} + 1) << size) - 1;
                next_addr = (current_addr & ~wrap_mask) |
                            ((current_addr + incr) & wrap_mask);
            end
            default: next_addr = current_addr + incr;
        endcase
        next_burst_addr = next_addr;
    end
    endfunction

    // ============================================================
    // Byte-Strobe Write Helper (unchanged)
    // ============================================================
    // verilator lint_off UNUSEDSIGNAL
    task automatic strobe_write(
        input  [ADDR_WIDTH-1:0]    addr,
        input  [DATA_WIDTH-1:0]    data,
        input  [STRB_WIDTH-1:0]    strb
    );
        integer b;
        reg [$clog2(MEM_DEPTH)-1:0] word_addr;
    begin
        word_addr = addr[$clog2(MEM_DEPTH)-1+$clog2(STRB_WIDTH) : $clog2(STRB_WIDTH)];
        for (b = 0; b < STRB_WIDTH; b = b + 1) begin
            if (strb[b])
                mem[word_addr][b*8 +: 8] <= data[b*8 +: 8];
        end
    end
    // verilator lint_on UNUSEDSIGNAL
    endtask

    // ============================================================
    // Memory Read Helper (pure function, no side effects)
    // ============================================================
    // verilator lint_off UNUSEDSIGNAL
    function automatic [DATA_WIDTH-1:0] mem_read(
        input [ADDR_WIDTH-1:0] addr
    );
        reg [$clog2(MEM_DEPTH)-1:0] word_addr;
    begin
        word_addr = addr[$clog2(MEM_DEPTH)-1+$clog2(STRB_WIDTH) : $clog2(STRB_WIDTH)];
        mem_read = mem[word_addr];
    end
    endfunction
    // verilator lint_on UNUSEDSIGNAL

    // ============================================================
    // Write Channel FSM — FIXED per AMBA AXI4 Spec
    // ============================================================
    //
    // Fixes applied (same pattern as read FSM):
    //
    //   W_FIX 1: AWREADY driven HIGH unconditionally in W_IDLE.
    //            No combinational dependency on AWVALID.
    //            AXI4 Spec A3.2.1: "A destination can assert READY
    //            before VALID is asserted."
    //
    //   W_FIX 2: AW signals latched ONLY on (AWVALID && AWREADY).
    //            Not on AWVALID alone.
    //
    //   W_FIX 3: BVALID driven unconditionally in W_WRITE_RESP.
    //            Handshake check uses canonical (BVALID && BREADY).
    //            BID/BRESP assigned once at state entry (not every
    //            cycle) to avoid spurious toggles.
    //
    // ============================================================
    always @(posedge ACLK or negedge ARESETn) begin
        if (!ARESETn) begin
            AWREADY      <= 1'b0;
            WREADY       <= 1'b0;
            BVALID       <= 1'b0;
            BID          <= {ID_WIDTH{1'b0}};
            BRESP        <= RESP_OKAY;
            wr_en        <= 1'b0;
            burst_addr   <= {ADDR_WIDTH{1'b0}};
            burst_len    <= 8'd0;
            burst_size   <= 3'd0;
            burst_type   <= 2'b00;
            write_id     <= {ID_WIDTH{1'b0}};
            beat_counter <= 8'd0;
            wlast_error  <= 1'b0;
            write_state  <= W_IDLE;
        end else begin
            wr_en <= 1'b0;
            case (write_state)

                // =============================================
                // W_IDLE: Wait for write address handshake
                // =============================================
                W_IDLE: begin
                    BVALID  <= 1'b0;
                    WREADY  <= 1'b0;

                    // ─────────────────────────────────────────
                    // W_FIX 1: AWREADY driven HIGH unconditionally.
                    // Slave is ready to accept an address at any time
                    // while idle. No dependency on AWVALID.
                    // ─────────────────────────────────────────
                    AWREADY <= 1'b1;

                    // ─────────────────────────────────────────
                    // W_FIX 2: Latch ONLY on (AWVALID && AWREADY).
                    // This is the canonical AXI4 handshake. Since
                    // AWREADY is unconditionally 1 in this state,
                    // the guard effectively checks AWVALID — but
                    // is written explicitly for protocol correctness.
                    // ─────────────────────────────────────────
                    if (AWVALID && AWREADY) begin
                        burst_addr   <= AWADDR;
                        burst_len    <= AWLEN;
                        burst_size   <= AWSIZE;
                        burst_type   <= AWBURST;
                        write_id     <= AWID;
                        beat_counter <= 8'd0;
                        wlast_error  <= 1'b0;
                        write_state  <= W_WRITE_DATA;
                    end
                    // No else needed — AWREADY stays HIGH unconditionally
                end

                // =============================================
                // W_WRITE_DATA: Accept burst data beats
                // =============================================
                W_WRITE_DATA: begin
                    // AWREADY must be LOW during active burst
                    AWREADY <= 1'b0;
                    // WREADY unconditionally HIGH — ready to accept data
                    WREADY  <= 1'b1;

                    if (WVALID && WREADY) begin
                        strobe_write(burst_addr, WDATA, WSTRB);
                        if (burst_addr[7:0] == 8'h00 && !fifo_full)
                            wr_en <= 1'b1;

                        if (beat_counter == burst_len) begin
                            // Final beat
                            if (!WLAST) begin
                                wlast_error <= 1'b1;
                                $display("[AXI4 WRITE] PROTOCOL ERROR: WLAST=0 on final beat %0d. Time=%0t",
                                         beat_counter, $time);
                            end
                            WREADY      <= 1'b0;
                            write_state <= W_WRITE_RESP;
                        end else begin
                            if (WLAST) begin
                                // Early WLAST — protocol error
                                wlast_error <= 1'b1;
                                $display("[AXI4 WRITE] PROTOCOL ERROR: WLAST=1 on beat %0d of %0d. Time=%0t",
                                         beat_counter, burst_len, $time);
                                WREADY      <= 1'b0;
                                write_state <= W_WRITE_RESP;
                            end else begin
                                // Normal beat — advance counter and address
                                beat_counter <= beat_counter + 8'd1;
                                burst_addr   <= next_burst_addr(
                                    burst_addr, burst_size, burst_type, burst_len);
                            end
                        end
                    end
                end

                // =============================================
                // W_WRITE_RESP: Drive write response
                // =============================================
                // W_FIX 3: BVALID unconditionally HIGH.
                // AXI4 Spec A3.2.1: once VALID is asserted it
                // must remain until handshake.
                // =============================================
                W_WRITE_RESP: begin
                    WREADY <= 1'b0;
                    BVALID <= 1'b1;
                    BID    <= write_id;
                    BRESP  <= wlast_error ? RESP_SLVERR : RESP_OKAY;

                    if (BVALID && BREADY) begin
                        BVALID      <= 1'b0;
                        write_state <= W_IDLE;
                    end
                end

                // =============================================
                // Default: safe recovery
                // =============================================
                default: begin
                    AWREADY     <= 1'b0;
                    WREADY      <= 1'b0;
                    BVALID      <= 1'b0;
                    BRESP       <= RESP_SLVERR;
                    wr_en       <= 1'b0;
                    write_state <= W_IDLE;
                end
            endcase
        end
    end


    // ════════════════════════════════════════════════════════════
    //
    //   FIXED AXI4 READ BURST IMPLEMENTATION
    //
    // ════════════════════════════════════════════════════════════


    // ── Read channel registers ──

    reg [ADDR_WIDTH-1:0]  read_addr;       // Current beat address
    reg [7:0]             read_len;        // Latched ARLEN (clamped for FIFO region)
    reg [2:0]             read_size;       // Latched ARSIZE
    reg [1:0]             read_burst;      // Latched ARBURST
    reg [ID_WIDTH-1:0]    read_id;         // Latched ARID → driven on RID
    reg [7:0]             read_counter;    // Completed beats (0..read_len)
    reg [1:0]             read_state;      // FSM state

    // ── FIX 4: Registered data pipeline ──
    // rdata_next holds the NEXT beat's data. It is computed
    // combinationally but registered into RDATA on the next
    // posedge. This eliminates the combinational path from
    // ARADDR → mem_read → RDATA that existed in the old design.
    reg [DATA_WIDTH-1:0]  rdata_next;      // Staged next-beat data
    reg [1:0]             rresp_next;      // Staged next-beat response

    // ── FIX 9: Pre-computed next address (datapath wire) ──
    // Pure combinational — no side effects. Used by both the
    // handshake advance path and the data staging path.
    wire [ADDR_WIDTH-1:0] read_addr_next;
    assign read_addr_next = next_burst_addr(
        read_addr, read_size, read_burst, read_len
    );

    // ── FIX 3: Unified RLAST — ONE canonical condition ──
    //
    // Single source of truth for "final beat":
    //   read_is_final_beat = (read_counter == read_len)
    //
    // BUG_RLAST_EARLY modifies ONLY this wire:
    //   read_is_final_beat = (read_counter == read_len - 1)
    //
    // All RLAST assignments in R_BURST derive from this
    // one wire. No duplicate definitions anywhere.
    // ────────────────────────────────────────────────────
    wire read_is_final_beat = (BUG_RLAST_EARLY == 0)
        ? (read_counter == read_len)
        : (read_counter == read_len - 8'd1);

    // ── FIX 2: Region detection ──
    // Determines whether the target address is in the FIFO/register
    // space (< 0x100) or the memory space (>= 0x100).
    // FIFO region forces single-beat behavior to prevent mixed bursts.
    wire ar_is_fifo_region = (ARADDR[ADDR_WIDTH-1:8] == {(ADDR_WIDTH-8){1'b0}});


    // ============================================================
    // Read Channel FSM
    // ============================================================
    //
    //  R_IDLE ──(ARVALID && ARREADY)──> R_BURST
    //     • ARREADY is HIGH unconditionally (FIX 1)
    //     • On handshake: latch AR*, compute rdata_next,
    //       set read_counter, clamp read_len for FIFO region
    //     • RVALID remains LOW — data appears next cycle
    //
    //  R_BURST
    //     • ARREADY = 0 (no new address mid-burst) (FIX 8)
    //     • RVALID = 1
    //     • RDATA driven from rdata_next (registered) (FIX 4)
    //     • On (RVALID && RREADY) handshake:
    //         - If final beat: → R_IDLE
    //         - Else: increment counter, advance address,
    //           stage next rdata_next (FIX 6)
    //     • On backpressure (RREADY=0):
    //         - ALL outputs HOLD (FIX 5)
    //         - Counter/address FROZEN
    //
    // ============================================================

    always @(posedge ACLK or negedge ARESETn) begin
        if (!ARESETn) begin
            // ── Reset: all read channel state cleared ──
            ARREADY      <= 1'b0;
            RVALID       <= 1'b0;
            RDATA        <= {DATA_WIDTH{1'b0}};
            RRESP        <= RESP_OKAY;
            RID          <= {ID_WIDTH{1'b0}};
            RLAST        <= 1'b0;
            rd_en        <= 1'b0;
            read_addr    <= {ADDR_WIDTH{1'b0}};
            read_len     <= 8'd0;
            read_size    <= 3'd0;
            read_burst   <= 2'b00;
            read_id      <= {ID_WIDTH{1'b0}};
            read_counter <= 8'd0;
            read_state   <= R_IDLE;
            rdata_next   <= {DATA_WIDTH{1'b0}};
            rresp_next   <= RESP_OKAY;
        end else begin
            rd_en <= 1'b0;   // default: FIFO read strobe off

            case (read_state)

                // =============================================
                // R_IDLE: Waiting for read address handshake
                // =============================================
                R_IDLE: begin
                    // ─────────────────────────────────────────
                    // FIX 8: RVALID is ONLY asserted in R_BURST.
                    // In IDLE, it is unconditionally LOW.
                    // ─────────────────────────────────────────
                    RVALID <= 1'b0;

                    // ─────────────────────────────────────────
                    // FIX 1: ARREADY driven HIGH unconditionally
                    // in R_IDLE — no dependence on ARVALID.
                    //
                    // AXI4 Spec A3.2.1: "A slave can assert
                    // READY before VALID is asserted."
                    //
                    // The handshake is detected by checking
                    // (ARVALID && ARREADY) BELOW, which gates
                    // ALL latching operations.
                    // ─────────────────────────────────────────
                    ARREADY <= 1'b1;

                    // ─────────────────────────────────────────
                    // FIX 1: Latch ONLY on (ARVALID && ARREADY).
                    // Since ARREADY is unconditionally 1 in this
                    // state, this simplifies to checking ARVALID.
                    // But the guard is written explicitly as the
                    // canonical handshake form for correctness.
                    // ─────────────────────────────────────────
                    if (ARVALID && ARREADY) begin

                        // ── Latch address-phase fields ──
                        read_addr  <= ARADDR;
                        read_size  <= ARSIZE;
                        read_burst <= ARBURST;
                        read_id    <= ARID;

                        // ─────────────────────────────────────
                        // FIX 2: FIFO/register region isolation.
                        //
                        // Addresses < 0x100 are FIFO or status
                        // registers. They MUST be treated as
                        // single-beat regardless of ARLEN.
                        //
                        // This prevents the bug where beat 0
                        // reads a FIFO entry and beat 1+ reads
                        // from the memory array — a mixed burst
                        // that corrupts data.
                        //
                        // Memory region (>= 0x100): use ARLEN
                        // as-is for full burst support.
                        // ─────────────────────────────────────
                        if (ar_is_fifo_region)
                            read_len <= 8'd0;        // force single-beat
                        else
                            read_len <= ARLEN;        // full burst

                        // ─────────────────────────────────────
                        // BUG_RD_COUNTER_INIT: off-by-one bug.
                        // Correct: 0. Buggy: 1.
                        // When enabled, the counter starts at 1,
                        // causing the FSM to think it's one beat
                        // ahead → premature burst termination.
                        // ─────────────────────────────────────
                        read_counter <= BUG_RD_COUNTER_INIT[7:0];

                        // ─────────────────────────────────────
                        // FIX 4: Compute first beat's data into
                        // the rdata_next staging register.
                        //
                        // The data does NOT go directly onto RDATA
                        // in this cycle. Instead, R_BURST will
                        // transfer rdata_next → RDATA on entry.
                        // This eliminates the combinational path
                        // ARADDR → mem_read() → RDATA.
                        //
                        // FIX 9: FIFO/status reads computed here
                        // in the same staging register, keeping
                        // datapath unified.
                        // ─────────────────────────────────────
                        rresp_next <= RESP_OKAY;

                        casez (ARADDR[11:0])
                            12'h010: begin
                                // FIFO data pop
                                if (!fifo_empty) begin
                                    rd_en      <= 1'b1;
                                    rdata_next <= {{(DATA_WIDTH-FIFO_DWIDTH){1'b0}}, fifo_dout};
                                end else begin
                                    rdata_next <= 32'hDEAD_BEEF;
                                    rresp_next <= RESP_SLVERR;
                                end
                            end
                            12'h020: begin
                                rdata_next <= {{(DATA_WIDTH-7){1'b0}},
                                               fifo_count, fifo_full, fifo_empty};
                            end
                            12'h080: begin
                                rdata_next <= {{(DATA_WIDTH-5){1'b0}}, fifo_level};
                            end
                            default: begin
                                if (ARADDR[11:8] != 4'h0)
                                    rdata_next <= mem_read(ARADDR);
                                else begin
                                    rdata_next <= 32'hBAD0_BAD0;
                                    rresp_next <= 2'b11;  // DECERR
                                end
                            end
                        endcase

                        // ── Transition to burst delivery state ──
                        // ARREADY will be driven LOW on next cycle
                        // by the R_BURST state.
                        read_state <= R_BURST;

                    end // (ARVALID && ARREADY)

                    // Note: if ARVALID is low, ARREADY stays HIGH.
                    // No else branch needed — ARREADY is unconditional.
                end


                // =============================================
                // R_BURST: Active read burst — deliver data
                // =============================================
                R_BURST: begin

                    // ─────────────────────────────────────────
                    // FIX 8: ARREADY must be LOW during an active
                    // burst. No new address can be accepted until
                    // the current burst completes.
                    // ─────────────────────────────────────────
                    ARREADY <= 1'b0;

                    // ─────────────────────────────────────────
                    // FIX 8: RVALID unconditionally HIGH in this
                    // state. AXI4 Spec A3.2.1: "Once VALID is
                    // asserted it must remain asserted until the
                    // handshake occurs."
                    //
                    // RVALID is NOT gated by RREADY — that would
                    // create an illegal combinational dependency.
                    // ─────────────────────────────────────────
                    RVALID <= 1'b1;

                    // ─────────────────────────────────────────
                    // FIX 7: RID driven from latched read_id.
                    // Assigned ONCE here. Constant for all beats.
                    // AXI4 Spec A5.3.1: "RID must match the
                    // corresponding ARID value."
                    // ─────────────────────────────────────────
                    RID <= read_id;

                    // ─────────────────────────────────────────
                    // FIX 4: Transfer staged data to RDATA.
                    //
                    // On the first cycle in R_BURST (after AR
                    // handshake), this loads rdata_next that was
                    // computed in R_IDLE. On subsequent beats
                    // (after handshake), rdata_next was updated
                    // at the end of the previous handshake cycle.
                    //
                    // If backpressured (RVALID=1, RREADY=0), the
                    // handshake branch below does NOT execute, so
                    // rdata_next holds its previous value, and
                    // RDATA gets the same value again → stable.
                    // ─────────────────────────────────────────
                    RDATA <= rdata_next;
                    RRESP <= rresp_next;

                    // ─────────────────────────────────────────
                    // FIX 3: RLAST from the ONE canonical source.
                    //
                    // read_is_final_beat =
                    //   (read_counter == read_len) [correct]
                    //   or (read_counter == read_len-1) [buggy]
                    //
                    // This is the ONLY place RLAST is assigned
                    // in R_BURST. No duplicates.
                    // ─────────────────────────────────────────
                    RLAST <= read_is_final_beat;

                    // ═══════════════════════════════════════
                    // Handshake guard: (RVALID && RREADY)
                    // ═══════════════════════════════════════
                    // FIX 6: ALL state updates gated by this.
                    // RVALID is always 1 in this state, so
                    // the effective guard is just RREADY.
                    // But we write it canonically for clarity.
                    // ═══════════════════════════════════════
                    if (RVALID && RREADY) begin

                        if (read_is_final_beat) begin
                            // ─────────────────────────────────
                            // Final beat accepted → burst done.
                            //
                            // Transition back to R_IDLE. RVALID
                            // will go LOW on next cycle (R_IDLE
                            // drives it unconditionally LOW).
                            // RLAST will also go LOW.
                            // ─────────────────────────────────
                            read_state <= R_IDLE;

                        end else begin
                            // ─────────────────────────────────
                            // Not the final beat → advance.
                            // ─────────────────────────────────

                            // FIX 6: Counter increment — ONLY here.
                            read_counter <= read_counter + 8'd1;

                            // FIX 6: Address advance — ONLY here.
                            read_addr <= read_addr_next;

                            // FIX 4/9: Stage NEXT beat's data.
                            // read_addr_next is a pure wire —
                            // the address that will be current
                            // on the next cycle.
                            rdata_next <= mem_read(read_addr_next);
                            rresp_next <= RESP_OKAY;
                        end

                    end else begin
                        // ─────────────────────────────────────
                        // FIX 5: Backpressure — RREADY = 0.
                        //
                        // AXI4 Spec A3.2.1: Source must not
                        // change RDATA, RLAST, RID, RRESP, or
                        // deassert RVALID before the handshake.
                        //
                        // RVALID stays HIGH (assigned above).
                        // RDATA  = rdata_next (unchanged since
                        //          no handshake updated it).
                        // RLAST  = read_is_final_beat (unchanged
                        //          since counter didn't move).
                        // RID    = read_id (constant).
                        //
                        // read_counter : NOT incremented.
                        // read_addr    : NOT advanced.
                        // rdata_next   : NOT updated.
                        //
                        // BUG_RD_ADDR_NO_HSHK: When enabled,
                        // the address advances even without a
                        // handshake — violating backpressure.
                        // This ONLY affects read_addr; counter
                        // and rdata_next are still frozen, so
                        // the data presented to the master is
                        // stale while the internal pointer has
                        // moved ahead.
                        // ─────────────────────────────────────
                        if (BUG_RD_ADDR_NO_HSHK) begin
                            read_addr <= read_addr_next;
                            $display("[AXI4 READ BUG] Address advanced without RREADY handshake at time %0t", $time);
                        end
                        // else: everything holds — correct.
                    end
                end

                // =============================================
                // Default: safe recovery to IDLE
                // =============================================
                default: begin
                    ARREADY      <= 1'b0;
                    RVALID       <= 1'b0;
                    RDATA        <= 32'hBAD0_BAD0;
                    RRESP        <= 2'b11;
                    RID          <= {ID_WIDTH{1'b0}};
                    RLAST        <= 1'b0;
                    rd_en        <= 1'b0;
                    read_state   <= R_IDLE;
                    rdata_next   <= {DATA_WIDTH{1'b0}};
                    rresp_next   <= RESP_OKAY;
                end

            endcase
        end
    end

endmodule


