# Define external clock
create_clock -period 20.0 [get_ports clk]

# Enhance the reliability of USB BlasterII, from DE10-Standard Examples
create_clock -name {altera_reserved_tck} -period 40 {altera_reserved_tck}
set_input_delay -clock altera_reserved_tck -clock_fall 3 [get_ports altera_reserved_tdi]
set_input_delay -clock altera_reserved_tck -clock_fall 3 [get_ports altera_reserved_tms]
set_output_delay -clock altera_reserved_tck 3 [get_ports altera_reserved_tdo]

derive_pll_clocks

# Define generated clock for SDRAM
create_generated_clock -source [get_pins { inst|pll|altera_pll_i|general[1].gpll~PLL_OUTPUT_COUNTER|divclk }] -name clk_sdram_ext [get_ports {sdram_clk}]

derive_clock_uncertainty

# SDRAM timing parameters, from DE10-Standard Examples
set_input_delay -max -clock clk_sdram_ext 5.96 [get_ports sdram_dq*]
set_input_delay -min -clock clk_sdram_ext 2.97 [get_ports sdram_dq*]

set_multicycle_path -from [get_clocks {clk_sdram_ext}] -to [get_clocks {inst|pll|altera_pll_i|general[0].gpll~PLL_OUTPUT_COUNTER|divclk }] -setup 2

set_output_delay -max -clock clk_sdram_ext 1.63  [get_ports {sdram_dq* sdram_dqm*}]
set_output_delay -min -clock clk_sdram_ext -0.95 [get_ports {sdram_dq* sdram_dqm*}]
set_output_delay -max -clock clk_sdram_ext 1.65  [get_ports {sdram_addr* sdram_ba* sdram_ras_n sdram_cas_n sdram_we_n sdram_cke sdram_cs_n}]
set_output_delay -min -clock clk_sdram_ext -0.9 [get_ports {sdram_addr* sdram_ba* sdram_ras_n sdram_cas_n sdram_we_n sdram_cke sdram_cs_n}]

# Asynchronous input
set_false_path -from [get_ports {rst_n}] -to *
