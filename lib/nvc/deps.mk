# Generated by nvc 1.13-devel

lib/nvc/NVC.POLYFILL-body: lib/std/STD.STANDARD lib/nvc/NVC.POLYFILL $(top_srcdir)/lib/nvc/polyfill-body.vhd

lib/nvc/NVC.POLYFILL: lib/std/STD.STANDARD $(top_srcdir)/lib/nvc/polyfill.vhd

lib/nvc/NVC.PSL_SUPPORT-body: lib/std/STD.STANDARD lib/nvc/NVC.PSL_SUPPORT lib/ieee/IEEE.STD_LOGIC_1164 $(top_srcdir)/lib/nvc/psl_support-body.vhd

lib/nvc/NVC.PSL_SUPPORT: lib/std/STD.STANDARD lib/ieee/IEEE.STD_LOGIC_1164 $(top_srcdir)/lib/nvc/psl_support.vhd

lib/nvc/NVC.SIM_PKG-body: lib/std/STD.STANDARD lib/nvc/NVC.SIM_PKG $(top_srcdir)/lib/nvc/sim_pkg-body.vhd

lib/nvc/NVC.SIM_PKG: lib/std/STD.STANDARD $(top_srcdir)/lib/nvc/sim_pkg.vhd

lib/nvc/NVC.TEXT_UTIL-body: lib/std/STD.STANDARD lib/nvc/NVC.TEXT_UTIL lib/nvc/NVC.POLYFILL $(top_srcdir)/lib/nvc/text_util-body.vhd

lib/nvc/NVC.TEXT_UTIL: lib/std/STD.STANDARD $(top_srcdir)/lib/nvc/text_util.vhd

lib/nvc/NVC.VERILOG-body: lib/std/STD.STANDARD lib/nvc/NVC.VERILOG lib/nvc/NVC.POLYFILL lib/ieee/IEEE.STD_LOGIC_1164 $(top_srcdir)/lib/nvc/verilog-body.vhd

lib/nvc/NVC.VERILOG: lib/std/STD.STANDARD lib/ieee/IEEE.STD_LOGIC_1164 $(top_srcdir)/lib/nvc/verilog.vhd

