BOOST_ROOT = $(HOME)/boost-1.81

CXX = g++-12
CXXFLAGS = -I$(BOOST_ROOT)/include \
           -std=c++20 \
           -march=native \
           -O3 -g \
           -Wall -pedantic \
           -Wno-ignored-attributes \
           -fopenmp \
           -msse2 -mssse3 -maes -mpclmul \
           -DBOOST_ERROR_CODE_HEADER_ONLY \
           -DBOOST_ASIO_HAS_CO_AWAIT \
           -pthread

LIBS = -lbsd

# ---------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------

MPCOPS_TEST = mpcops_test
MPCOPS_SRC  = mpcops_test.cpp

REMISE      = remise
REMISE_SRC  = remise.cpp

# ---------------------------------------------------------------------
# Default rule
# ---------------------------------------------------------------------

all: $(MPCOPS_TEST) $(REMISE)

# ---------------------------------------------------------------------
# MPC Operations Test
# ---------------------------------------------------------------------

$(MPCOPS_TEST): $(MPCOPS_SRC) shares.hpp mpcops.hpp types.hpp
	$(CXX) $(CXXFLAGS) $(MPCOPS_SRC) -o $(MPCOPS_TEST) $(LIBS)

# ---------------------------------------------------------------------
# Remise
# ---------------------------------------------------------------------

$(REMISE): $(REMISE_SRC) dpf.hpp types.hpp network.hpp
	$(CXX) $(CXXFLAGS) $(REMISE_SRC) -o $(REMISE) $(LIBS)

# ---------------------------------------------------------------------
# Run helpers
# ---------------------------------------------------------------------

run-mpcops: $(MPCOPS_TEST)
	./$(MPCOPS_TEST)

run-remise: $(REMISE)
	./$(REMISE)

# ---------------------------------------------------------------------
# Clean
# ---------------------------------------------------------------------

clean:
	rm -f $(MPCOPS_TEST) $(REMISE)