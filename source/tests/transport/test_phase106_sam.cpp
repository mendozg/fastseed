#include "fastseed/transport/transport.hpp"
#include <boost/asio.hpp>
#include <cassert>
#include <iostream>
#include <thread>
#include <string>
using tcp=boost::asio::ip::tcp;
static std::string read_line(tcp::socket& s){boost::asio::streambuf b;boost::asio::read_until(s,b,'\n');std::istream in(&b);std::string x;std::getline(in,x);return x;}
int main(){boost::asio::io_context sio;tcp::acceptor a(sio,{tcp::v4(),0});auto port=a.local_endpoint().port();std::thread th([&]{tcp::socket s(sio);a.accept(s);assert(read_line(s).find("HELLO VERSION")==0);boost::asio::write(s,boost::asio::buffer(std::string("HELLO REPLY RESULT=OK VERSION=3.3\n")));assert(read_line(s).find("SESSION CREATE")==0);boost::asio::write(s,boost::asio::buffer(std::string("SESSION STATUS RESULT=OK\n")));assert(read_line(s).find("STREAM CONNECT")==0);boost::asio::write(s,boost::asio::buffer(std::string("STREAM STATUS RESULT=OK\n")));});
 boost::asio::io_context io;fastseed::transport::I2pSamTransport tr(io,{"127.0.0.1",port},[](const fastseed::crawler::PeerCandidate&){return std::string("BASE64DEST");});auto c=fastseed::crawler::PeerCandidate{};c.network=fastseed::crawler::CandidateNetwork::I2P;c.host="i2p:";c.port=8226;c.address_bytes.resize(32,2);bool called=false;fastseed::transport::TransportResult r;tr.connect(c,std::chrono::seconds(2),[&](auto x){called=true;r=std::move(x);});io.run();th.join();assert(called);assert(r.status==fastseed::transport::TransportStatus::Connected);assert(r.socket);std::cout<<"phase10.6 SAM mock integration PASS\n";}
