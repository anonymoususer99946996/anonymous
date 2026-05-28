#include "dpf++/dpf.h"
#include "lowmc/lowmc.h"
#include <iostream>
#include "lowmc/constants_b128_r29_s11.h"
 
#include <cstdlib>
 
 

#include "block.h"
#include <chrono>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>
#include <boost/asio.hpp>

#include <chrono>
#include <iostream>

using namespace std::chrono;

using boost::asio::ip::tcp;

#include "lowmc/streams.h"

#include <mutex>
#include <boost/lexical_cast.hpp>
#include "network.h"
 
using LowMC = lowmc::lowmc<128,29,11>;
using bitsliced_LowMC = lowmc::bitsliced_lowmc<128, 29, 11, 256>;
using block_t = LowMC::block_t;

typedef lowmc::streams::socket_stream<block_t> sock;
typedef unsigned char byte_t;
typedef __m128i leaf_t;
typedef __m128i node_t;
const size_t sboxes = LowMC::sboxes_per_round;
 
typedef lowmc::lowmc<128,29, 11> prgkey_t;

 

const leaf_t val =  _mm_set1_epi8(0x01);

//using namespace dpf;


const size_t rounds = LowMC::num_rounds;

using instream     = LowMC::instream;
using outstream    = LowMC::outstream;

//using insocketstream     = LowMC::inputsocketstream;
//using outsocketstream    = LowMC::output_socket_stream;

using rewindstream =  LowMC::rewindstream;
using basicstream  = LowMC::basicstream;
using inputsocketstream  = LowMC::inputsocketstream;

 
 
void prg_mpc2(sock& sb,  const LowMC prgkey, const block_t& seed, __m128i &outbuf,  bool party)
{
 
 
	 auto start = high_resolution_clock::now();

	 outbuf = prgkey.encrypt2_p0p1(seed, sb, party); 

	

auto end = high_resolution_clock::now();


auto duration_ms = duration_cast<milliseconds>(end - start).count();

std::cout << "Time: "
          << duration_ms << " ms"
          << std::endl;

}


 

 
 

 
 
 
 


int main(int argc, char * argv[])
{ 	

 

	boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);
 	std::string addr = "127.0.0.1";
 	// const std::string host1 = (argc < 2) ? "127.0.0.1" : argv[1];
 //    const std::string host2 = (argc < 3) ? "127.0.0.1" : argv[2];
 //    const std::string host3 = (argc < 4) ? "127.0.0.1" : argv[3];
   	bool party;

   	#if (PARTY == 0)    
     //sock in2(tcp::socket(io_context), resolver, "localhost", std::to_string(PORT_P0_P2));	// to receive from P2
     sock sb(tcp::socket(io_context), resolver, "127.0.0.1", std::to_string(PORT_P1_P0)); // to receive from P1 
     party = false; 	 
 	#else	
		//sock in2(tcp::socket(io_context), resolver, "localhost", std::to_string(PORT_P1_P2)); // to receive from P2
		tcp::acceptor __acceptor(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0));  // to write to P1
        sock sb(__acceptor.accept());
    	party = true;
    	usleep(20000);
 	#endif


 
  

   	printf("Connections established\n");

 

 


 
 
	size_t target;
	arc4random_buf(&target, sizeof(size_t));
	target = target % nitems;
	node_t root[2];

	arc4random_buf(root, sizeof(node_t) * 2);


  
 
	LowMC prgkey;
	prgkey.maska.shiftr(prgkey.identity_len  - 1);
	prgkey.maskb = prgkey.maska >> 1;
	/// mask for low-order bit in each s-box
	prgkey.maskc = prgkey.maska >> 2;
	/// mask for the all-but-the-highest-order bit in each s-box
	prgkey.maskbc = prgkey.maskb | prgkey.maskc;
	printf("maska = \n");

 


	__m128i out0;
	block<__m128i> seed = root[0];



	block<__m128i> seed_recv;

	sb << seed;
	
	sb >> seed_recv;

	seed_recv.mX = seed_recv.mX ^ seed.mX;

	auto out_ = prgkey.encrypt(seed_recv);

	//std::cout << "out_ = " << out_[0] << " " << out_[1] << std::endl;

	//prg_mpc2( sb, prgkey, seed, out0, party); 


prg_mpc2(sb, prgkey, seed, out0, party);


	block<__m128i> out0_recv;

	sb << out0;
	sb >> out0_recv;



	out0_recv.mX ^= out0;



    printf("%llu <> %llu", out_.mX[0], out0_recv.mX[0]);

 	printf("PARTY = %d\n", party);

 
	
	return 0;
}
