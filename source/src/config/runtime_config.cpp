#include "fastseed/config/runtime_config.hpp"
#include <fstream>
#include <regex>
#include <sstream>
namespace fastseed::config {
static bool has_section(const std::string& s,const std::string& sec){return s.find("["+sec+"]")!=std::string::npos;}
ValidationResult validate_file(const std::string& path){
 ValidationResult r; std::ifstream in(path); if(!in){r.errors.push_back("cannot open configuration: "+path);return r;}
 std::ostringstream buf; buf<<in.rdbuf(); auto s=buf.str();
 if(!has_section(s,"dns")) r.errors.push_back("missing [dns] section");
 if(!has_section(s,"crawler")) r.errors.push_back("missing [crawler] section");
 if(!has_section(s,"cache")) r.errors.push_back("missing [cache] section");
 if(!has_section(s,"health")) r.errors.push_back("missing [health] section");
 if(!has_section(s,"database")) r.errors.push_back("missing [database] section");
 if(s.find("listen_port")!=std::string::npos && s.find("listen_port = 0")!=std::string::npos) r.errors.push_back("dns.listen_port must be non-zero");
 if(s.find("max_response_size = 0")!=std::string::npos) r.errors.push_back("dns.max_response_size must be non-zero");
 if(s.find("max_concurrent_probes = 0")!=std::string::npos) r.errors.push_back("crawler.max_concurrent_probes must be non-zero");
 if(s.find("max_open_connections = 0")!=std::string::npos) r.errors.push_back("crawler.max_open_connections must be non-zero");
 if(s.find("path = \"\"")!=std::string::npos) r.errors.push_back("database.path must not be empty");
 r.ok=r.errors.empty(); return r;
}
std::string version_string(){return "BSTY-FastSeed 1.0.0 (Production)";}
}
