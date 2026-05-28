#ifndef NETWORK_H__
#define NETWORK_H__
 
#include <memory>
#include <boost/asio.hpp>
 
using boost::asio::ip::tcp;
typedef std::shared_ptr<tcp::socket> socket_ptr;

  #define PORT_P1_P0 2000
  #define PORT_P0_P1 2001
  #define PORT_P0_P2 2002  
  #define PORT_P1_P2 2003
  #define PORT_P0_P3 2004
  #define PORT_P1_P3 2005
  #define PORT_P3_P2 2006


  #define PORT_P1_P0_a 2007
  #define PORT_P0_P1_a 2008
  #define PORT_P0_P2_a 2009  
  #define PORT_P1_P2_a 2010
  #define PORT_P0_P3_a 2011
  #define PORT_P1_P3_a 2012
  #define PORT_P3_P2_a 2013


  #define PORT_P1_P0_b 2014
  #define PORT_P0_P1_b 2015
  #define PORT_P0_P2_b 2016  
  #define PORT_P1_P2_b 2017
  #define PORT_P0_P3_b 2018
  #define PORT_P1_P3_b 2019
  #define PORT_P3_P2_b 2020


  #define PORT_P1_P0_c 2021
  #define PORT_P0_P1_c 2022
  #define PORT_P0_P2_c 2023  
  #define PORT_P1_P2_c 2024
  #define PORT_P0_P3_c 2025
  #define PORT_P1_P3_c 2026
  #define PORT_P3_P2_c 2027


  #define PORT_P1_P0_d 2028
  #define PORT_P0_P1_d 2029
  #define PORT_P0_P2_d 2030  
  #define PORT_P1_P2_d 2031
  #define PORT_P0_P3_d 2032
  #define PORT_P1_P3_d 2033
  #define PORT_P3_P2_d 2034


  #define PORT_P1_P0_e 2035
  #define PORT_P0_P1_e 2036
  #define PORT_P0_P2_e 2037  
  #define PORT_P1_P2_e 2038
  #define PORT_P0_P3_e 2039
  #define PORT_P1_P3_e 2040
  #define PORT_P3_P2_e 2041 

  #define PORT_P1_P0_f 2042
  #define PORT_P0_P1_f 2043
  #define PORT_P0_P2_f 2044  
  #define PORT_P1_P2_f 2045
  #define PORT_P0_P3_f 2046
  #define PORT_P1_P3_f 2047
  #define PORT_P3_P2_f 2048 

  #define PORT_P1_P0_g 2049
  #define PORT_P0_P1_g 2050
  #define PORT_P0_P2_g 2051  
  #define PORT_P1_P2_g 2052
  #define PORT_P0_P3_g 2053
  #define PORT_P1_P3_g 2054
  #define PORT_P3_P2_g 2055 
 
  #define PORT_P1_P0_h 2056
  #define PORT_P0_P1_h 2057
  #define PORT_P0_P2_h 2058  
  #define PORT_P1_P2_h 2059
  #define PORT_P0_P3_h 2060
  #define PORT_P1_P3_h 2061
  #define PORT_P3_P2_h 2062

  #define PORT_P1_P0_i 2063
  #define PORT_P0_P1_i 2064
  #define PORT_P0_P2_i 2065  
  #define PORT_P1_P2_i 2066
  #define PORT_P0_P3_i 2067
  #define PORT_P1_P3_i 2068
  #define PORT_P3_P2_i 2069


  #define PORT_P1_P0_j 2070
  #define PORT_P0_P1_j 2071
  #define PORT_P0_P2_j 2072  
  #define PORT_P1_P2_j 2073
  #define PORT_P0_P3_j 2074
  #define PORT_P1_P3_j 2075
  #define PORT_P3_P2_j 2076



  #define PORT_P1_P0_k 2077
  #define PORT_P0_P1_k 2078
  #define PORT_P0_P2_k 2079  
  #define PORT_P1_P2_k 2080
  #define PORT_P0_P3_k 2081
  #define PORT_P1_P3_k 2082
  #define PORT_P3_P2_k 2083


  #define PORT_P1_P0_l 2084
  #define PORT_P0_P1_l 2085
  #define PORT_P0_P2_l 2086  
  #define PORT_P1_P2_l 2087
  #define PORT_P0_P3_l 2088
  #define PORT_P1_P3_l 2089
  #define PORT_P3_P2_l 2090


  #define PORT_P1_P0_m 2091
  #define PORT_P0_P1_m 2092
  #define PORT_P0_P2_m 2093  
  #define PORT_P1_P2_m 2094
  #define PORT_P0_P3_m 2095
  #define PORT_P1_P3_m 2096
  #define PORT_P3_P2_m 2097


  #define PORT_P1_P0_n 2098
  #define PORT_P0_P1_n 2099
  #define PORT_P0_P2_n 3000  
  #define PORT_P1_P2_n 3001
  #define PORT_P0_P3_n 3002
  #define PORT_P1_P3_n 3003
  #define PORT_P3_P2_n 3004

  #define PORT_P1_P0_o 3005
  #define PORT_P0_P1_o 3006
  #define PORT_P0_P2_o 3007  
  #define PORT_P1_P2_o 3008
  #define PORT_P0_P3_o 3009
  #define PORT_P1_P3_o 3010
  #define PORT_P3_P2_o 3011

  #define PORT_P1_P0_p 3012
  #define PORT_P0_P1_p 3013
  #define PORT_P0_P2_p 3014  
  #define PORT_P1_P2_p 3015
  #define PORT_P0_P3_p 3006
  #define PORT_P1_P3_p 3017
  #define PORT_P3_P2_p 3018

  #define PORT_P1_P0_q 3019
  #define PORT_P0_P1_q 3020
  #define PORT_P0_P2_q 3021  
  #define PORT_P1_P2_q 3022
  #define PORT_P0_P3_q 3023
  #define PORT_P1_P3_q 3024
  #define PORT_P3_P2_q 3025



/////////////////////////////////////////

 
#ifdef P_ZERO
  #define PORT_P2  PORT_P0_P2
  #define PORT_IN  PORT_P1_P0  
  #define PORT_OUT PORT_P0_P1 
  #define SELF     "P0"
  #define PARTNER  "P1"

  #define PORT_P2_DB  PORT_P0_P2_DB
  #define PORT_IN_DB  PORT_P1_P0_DB  
  #define PORT_OUT_DB PORT_P0_P1_DB 
  #define SELF_DB     "P0_DB"
  #define PARTNER_DB  "P1_DB"

  #define PORT_P2_DB_a  PORT_P0_P2_DB_a
  #define PORT_IN_DB_a  PORT_P1_P0_DB_a  
  #define PORT_OUT_DB_a PORT_P0_P1_DB_a 



#elif defined(P_ONE)
  #define PORT_P2  PORT_P1_P2
  #define PORT_IN  PORT_P0_P1 
  #define PORT_OUT PORT_P1_P0
  #define SELF     "P1"
  #define PARTNER  "P0"

  #define PORT_P2_DB  PORT_P1_P2_DB
  #define PORT_IN_DB  PORT_P0_P1_DB
  #define PORT_OUT_DB PORT_P1_P0_DB

  #define PORT_P2_DB_a  PORT_P1_P2_DB_a
  #define PORT_IN_DB_a  PORT_P0_P1_DB_a
  #define PORT_OUT_DB_a PORT_P1_P0_DB_a

  #define SELF_DB     "P1_DB"
  #define PARTNER_DB  "P0_DB"

#endif

template <typename T>
inline void swap(tcp::socket & s, T & x, size_t size = sizeof(T))
{
  
  boost::asio::async_write(s, boost::asio::buffer(&x, sizeof(T)),
    [](boost::system::error_code /*ec*/, std::size_t /*length*/) {});
  
  boost::asio::read(s, boost::asio::buffer(&x, sizeof(T)));


}

template <typename T>
inline void merge(tcp::socket & s, T & x)
{
  boost::asio::async_write(s, boost::asio::buffer(&x, sizeof(T)),
    [](boost::system::error_code /*ec*/, std::size_t /*length*/) {});
  T x2;
  boost::asio::read(s, boost::asio::buffer(&x2, sizeof(T)));
  x += x2;
}

#endif