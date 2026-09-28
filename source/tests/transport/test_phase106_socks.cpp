#include "fastseed/transport/transport.hpp"
#include <boost/asio.hpp>
#include <cassert>
#include <iostream>
#include <thread>
using tcp=boost::asio::ip::tcp;
int main(){
 boost::asio::io_context server_io; tcp::acceptor acc(server_io,{tcp::v4(),0}); auto port=acc.local_endpoint().port();
 std::thread server([&]{tcp::socket s(server_io);acc.accept(s);unsigned char hello[3];boost::asio::read(s,boost::asio::buffer(hello));assert(hello[0]==5&&hello[1]==1&&hello[2]==0);unsigned char method[2]={5,0};boost::asio::write(s,boost::asio::buffer(method));unsigned char req[4];boost::asio::read(s,boost::asio::buffer(req));assert(req[0]==5&&req[1]==1&&req[3]==3);unsigned char len;boost::asio::read(s,boost::asio::buffer(&len,1));std::vector<unsigned char> rest(len+2);boost::asio::read(s,boost::asio::buffer(rest));unsigned char ok[10]={5,0,0,1,127,0,0,1,0x20,0x1E};boost::asio::write(s,boost::asio::buffer(ok));});
 boost::asio::io_context io; fastseed::transport::TorSocks5Transport tr(io,{"127.0.0.1",port}); auto c=fastseed::crawler::PeerCandidate{}; c.network=fastseed::crawler::CandidateNetwork::TorV3;c.host="onion:";c.port=8226;c.address_bytes.resize(32,1);
 bool called=false;fastseed::transport::TransportResult result;tr.connect(c,std::chrono::seconds(2),[&](auto r){called=true;result=std::move(r);});io.run();server.join();assert(called);assert(result.status==fastseed::transport::TransportStatus::Connected);assert(result.socket);
 std::cout<<"phase10.6 SOCKS5 mock integration PASS\n";
}
