# Independent compile smoke for the delivered CXL library.
# Prerequisite: ../build_cxl_lib.sh has produced ../lib/libcxl_switch_model.a
# and ../include/cxl_*.h.
CXX      ?= g++
CXXFLAGS  = -std=c++17 -O2 -Wall -Wextra -I../include
LDFLAGS   = -L../lib -lcxl_switch_model

all: cxl_smoke

cxl_smoke: cxl_smoke.cpp ../lib/libcxl_switch_model.a
	$(CXX) $(CXXFLAGS) cxl_smoke.cpp -o $@ $(LDFLAGS)

run: cxl_smoke
	./cxl_smoke

clean:
	rm -f cxl_smoke

.PHONY: all run clean
