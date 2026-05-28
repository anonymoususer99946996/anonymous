BLOCK_LEN = 128
ROUNDS    = 29
SBOXES    = 11
BOOST_ROOT = $(HOME)/boost-1.81
CXX = g++
CXXFLAGS = -I$(BOOST_ROOT)/include -g -march=native -std=c++17 -Wall -pedantic -fopenmp -O3 \
           -Wno-ignored-attributes -pthread

LIBS = -lbsd -lboost_system -lboost_thread -pthread
FLAGS = -DDPFPP_DO_LOWMC -DBOOST_ERROR_CODE_HEADER_ONLY \
        -DBLOCK_LEN=$(BLOCK_LEN) -DROUNDS=$(ROUNDS) -DSBOXES=$(SBOXES)
        #-DNDEBUG

OPENSSL_INCLUDE ?= -I/usr/include/openssl
CXXFLAGS += $(OPENSSL_INCLUDE)

OPENSSL_LIBS ?= -lcrypto
LIBS += $(OPENSSL_LIBS)

# --------------------------------------------------

all: p0 p1 gen_constants

# --------------------------------------------------
# Party-specific DUORAM binaries

p0: lowmc-test.cpp gen_constants lowmc/lowmc.h dpf++/dpf.h
	$(CXX) $(CXXFLAGS) -o p0 lowmc-test.cpp \
	$(LIBS) $(FLAGS) -DPARTY=0 -I.

p1: lowmc-test.cpp gen_constants lowmc/lowmc.h dpf++/dpf.h
	$(CXX) $(CXXFLAGS) -o p1 lowmc-test.cpp \
	$(LIBS) $(FLAGS) -DPARTY=1 -I.


# --------------------------------------------------
# Constants generation

gen_constants: lowmc/gen_constants.cpp
	$(CXX) $(CXXFLAGS) -o gen_constants lowmc/gen_constants.cpp \
	$(LIBS) $(FLAGS)
	@test -f lowmc/constants_b$(BLOCK_LEN)_r$(ROUNDS)_s$(SBOXES).h || \
	  ./gen_constants > lowmc/constants_b$(BLOCK_LEN)_r$(ROUNDS)_s$(SBOXES).h

# --------------------------------------------------

docs:
	doxygen Doxyfile

clean:
	rm -f p0 p1 p2 users gen_constants

veryclean: clean
	rm -f lowmc/constants_b*_r*_s*.h
	rm -rf doc
