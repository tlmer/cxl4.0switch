# Standalone CXL console (no HDL simulator required).
# Prerequisite: ../build_cxl_lib.sh has produced ../lib/libcxl_switch_model.a
# and ../include/cxl_*.h.
CXX       ?= g++
CXXFLAGS   = -std=c++17 -O2 -Wall -Wextra -I../include
LDFLAGS    = -L../lib -lcxl_switch_model

all: cxl_switch_console

cxl_switch_console: cxl_switch_console.cpp ../lib/libcxl_switch_model.a
	$(CXX) $(CXXFLAGS) cxl_switch_console.cpp -o $@ $(LDFLAGS)

run: cxl_switch_console
	./cxl_switch_console --run-all

clean:
	rm -f cxl_switch_console

.PHONY: all run clean
