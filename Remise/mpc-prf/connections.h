 #if (PARTY==0)
    // std::cout << "P_ZERO: " << PORT_P0_P1 << std::endl;


    

    tcp::socket s2(io_context);
    boost::asio::connect(s2, resolver.resolve({host2,
    std::to_string(PORT_P0_P2)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;
    usleep(20000);

    tcp::socket s2_a(io_context);
    boost::asio::connect(s2_a, resolver.resolve({host2,
    std::to_string(PORT_P0_P2_a)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;
    usleep(20000);

    tcp::socket s2_b(io_context);
    boost::asio::connect(s2_b, resolver.resolve({host2,
    std::to_string(PORT_P0_P2_b)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;
    usleep(20000);

    tcp::socket s2_c(io_context);
    boost::asio::connect(s2_c, resolver.resolve({host2,
    std::to_string(PORT_P0_P2_c)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;
    usleep(20000);

    tcp::socket s2_d(io_context);
    boost::asio::connect(s2_d, resolver.resolve({host2,
    std::to_string(PORT_P0_P2_d)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;
    usleep(20000);

    tcp::socket s2_e(io_context);
    boost::asio::connect(s2_e, resolver.resolve({host2,
    std::to_string(PORT_P0_P2_e)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;
    usleep(20000);

     tcp::socket s2_f(io_context);
    boost::asio::connect(s2_f, resolver.resolve({host2,
    std::to_string(PORT_P0_P2_f)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;
    usleep(20000);

     tcp::socket s2_g(io_context);
    boost::asio::connect(s2_g, resolver.resolve({host2,
    std::to_string(PORT_P0_P2_g)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;
    usleep(20000);

         tcp::socket s2_h(io_context);
    boost::asio::connect(s2_h, resolver.resolve({host2,
    std::to_string(PORT_P0_P2_h)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;
    usleep(20000);

 
    
    tcp::socket sb(io_context);
    boost::asio::connect(sb, resolver.resolve({host1,
    std::to_string(PORT_P1_P0)}));
    // std::cout << "P0: [Established connection P1 (1)]" << std::endl;
    usleep(20000);

    tcp::socket sb_a(io_context);
    boost::asio::connect(sb_a, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_a)}));
    // std::cout << "P0: [Established connection P1 (2)]" << std::endl;
    usleep(20000);
    
    tcp::socket sb_b(io_context);
    boost::asio::connect(sb_b, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_b)}));
    // std::cout << "P0: [Established connection P1 (3)]" << std::endl;
    usleep(20000);

    tcp::socket sb_c(io_context);
    boost::asio::connect(sb_c, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_c)}));
    // std::cout << "P0: [Established connection P1 (4)]" << std::endl;
    usleep(20000);


    tcp::socket sb_d(io_context);
    boost::asio::connect(sb_d, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_d)}));
    // std::cout << "P0: [Established connection P1 (5)]" << std::endl;
    usleep(20000);

    tcp::socket sb_e(io_context);
    boost::asio::connect(sb_e, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_e)}));
    // std::cout << "P0: [Established connection P1 (6)]" << std::endl;
    usleep(20000);

    tcp::socket sb_f(io_context);
    boost::asio::connect(sb_f, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_f)}));
    // std::cout << "P0: [Established connection P1 (7)]" << std::endl;
    usleep(20000);

    tcp::socket sb_g(io_context);
    boost::asio::connect(sb_g, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_g)}));
    // std::cout << "P0: [Established connection P1 (8)]" << std::endl;
    usleep(20000);

    tcp::socket sb_h(io_context);
    boost::asio::connect(sb_h, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_h)}));
    // std::cout << "P0: [Established connection P1 (9)]" << std::endl;
    usleep(20000);

    tcp::socket sb_i(io_context);
    boost::asio::connect(sb_i, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_i)}));
    // std::cout << "P0: [Established connection P1 (9)]" << std::endl;
    usleep(20000);

    tcp::socket sb_j(io_context);
    boost::asio::connect(sb_j, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_j)}));
    // std::cout << "P0: [Established connection P1 (9)]" << std::endl;
    usleep(20000);

    tcp::socket sb_k(io_context);
    boost::asio::connect(sb_k, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_k)}));
    // std::cout << "P0: [Established connection P1 (9)]" << std::endl;
    usleep(20000);

    tcp::socket sb_l(io_context);
    boost::asio::connect(sb_l, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_l)}));
    // std::cout << "P0: [Established connection P1 (9)]" << std::endl;
    usleep(20000);

    tcp::socket sb_m(io_context);
    boost::asio::connect(sb_m, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_m)}));
    // std::cout << "P0: [Established connection P1 (9)]" << std::endl;
    usleep(20000);

    tcp::socket sb_n(io_context);
    boost::asio::connect(sb_n, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_n)}));
    // std::cout << "P0: [Established connection P1 (9)]" << std::endl;
    usleep(20000);

    tcp::socket sb_o(io_context);
    boost::asio::connect(sb_o, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_o)}));
    // std::cout << "P0: [Established connection P1 (9)]" << std::endl;
    usleep(20000);

    tcp::socket sb_p(io_context);
    boost::asio::connect(sb_p, resolver.resolve({host1,
    std::to_string(PORT_P1_P0_p)}));
    // std::cout << "P0: [Established connection P1 (9)]" << std::endl;
    usleep(20000);
  #endif



  #if (PARTY==1)
    
    // std::cout << "P_ONE: " << PORT_P1_P0 << std::endl;
   
    
    usleep(20000);

    
    tcp::socket s2(io_context);
    boost::asio::connect(s2, resolver.resolve({host2,
    std::to_string(PORT_P1_P2)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;    
    usleep(20000); 


    tcp::socket s2_a(io_context);
    boost::asio::connect(s2_a, resolver.resolve({host2,
    std::to_string(PORT_P1_P2_a)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;    
    usleep(20000);


    tcp::socket s2_b(io_context);
    boost::asio::connect(s2_b, resolver.resolve({host2,
    std::to_string(PORT_P1_P2_b)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;    
    usleep(20000);

    tcp::socket s2_c(io_context);
    boost::asio::connect(s2_c, resolver.resolve({host2,
    std::to_string(PORT_P1_P2_c)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;    
    usleep(20000);

    tcp::socket s2_d(io_context);
    boost::asio::connect(s2_d, resolver.resolve({host2,
    std::to_string(PORT_P1_P2_d)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;    
    usleep(20000);

    tcp::socket s2_e(io_context);
    boost::asio::connect(s2_e, resolver.resolve({host2,
    std::to_string(PORT_P1_P2_e)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;    
    usleep(20000);

    tcp::socket s2_f(io_context);
    boost::asio::connect(s2_f, resolver.resolve({host2,
    std::to_string(PORT_P1_P2_f)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;    
    usleep(20000);


    tcp::socket s2_g(io_context);
    boost::asio::connect(s2_g, resolver.resolve({host2,
    std::to_string(PORT_P1_P2_g)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;    
    usleep(20000);


    tcp::socket s2_h(io_context);
    boost::asio::connect(s2_h, resolver.resolve({host2,
    std::to_string(PORT_P1_P2_h)}));
    // std::cerr << "P1: [Established connection to P2]" << std::endl;    
    usleep(20000);
 
    
 
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb(acceptor.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0 << std::endl;

    tcp::acceptor acceptor_a(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_a));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_a(acceptor_a.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_a << std::endl;    

    tcp::acceptor acceptor_b(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_b));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_b(acceptor_b.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_b << std::endl;

    tcp::acceptor acceptor_c(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_c));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_c(acceptor_c.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_c << std::endl;

    tcp::acceptor acceptor_d(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_d));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_d(acceptor_d.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_d << std::endl;

    tcp::acceptor acceptor_e(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_e));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_e(acceptor_e.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_e << std::endl;

    tcp::acceptor acceptor_f(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_f));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_f(acceptor_f.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_f << std::endl;

    tcp::acceptor acceptor_g(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_g));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_g(acceptor_g.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_g << std::endl;

    tcp::acceptor acceptor_h(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_h));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_h(acceptor_h.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_h << std::endl;

    tcp::acceptor acceptor_i(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_i));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_i(acceptor_i.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_i << std::endl;

    tcp::acceptor acceptor_j(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_j));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_j(acceptor_j.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_j << std::endl;

    tcp::acceptor acceptor_k(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_k));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_k(acceptor_k.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_k << std::endl;

    tcp::acceptor acceptor_l(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_l));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_l(acceptor_l.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_l << std::endl;


    tcp::acceptor acceptor_m(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_m));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_m(acceptor_m.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_m << std::endl;


    tcp::acceptor acceptor_n(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_n));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_n(acceptor_n.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_n << std::endl;


    tcp::acceptor acceptor_o(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_o));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_o(acceptor_o.accept());
    // std::cerr << "Listenting on port: " << PORT_P1_P0_o << std::endl;

    tcp::acceptor acceptor_p(io_context, tcp::endpoint(tcp::v4(), PORT_P1_P0_p));
    // std::cout << " --- > " << std::endl;
    tcp::socket sb_p(acceptor_p.accept());
    // // std::cerr << "Listenting on port: " << PORT_P1_P0_p << std::endl;


   #endif