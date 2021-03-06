#include "config.h"
#include "dnsparser.hh"
#include "sstuff.hh"
#include "misc.hh"
#include "dnswriter.hh"
#include "dnsrecords.hh"
#include <boost/format.hpp>
#ifndef RECURSOR
#include "statbag.hh"
#include "base64.hh"
StatBag S;
#endif

volatile bool g_ret; // make sure the optimizer does not get too smart
uint64_t g_totalRuns;

volatile bool g_stop;

void alarmHandler(int)
{
  g_stop=true;
}

template<typename C> void doRun(const C& cmd, int mseconds=100)
{
  struct itimerval it;
  it.it_value.tv_sec=mseconds/1000;
  it.it_value.tv_usec = 1000* (mseconds%1000);
  it.it_interval.tv_sec=0;
  it.it_interval.tv_usec=0;

  signal(SIGVTALRM, alarmHandler);
  setitimer(ITIMER_VIRTUAL, &it, 0);
  
  unsigned int runs=0;
  g_stop=false;
  CPUTime dt;
  dt.start();
  while(runs++, !g_stop) {
    cmd();
  }
  double delta=dt.ndiff()/1000000000.0;
  boost::format fmt("'%s' %.02f seconds: %.1f runs/s, %.02f usec/run");

  cerr<< (fmt % cmd.getName() % delta % (runs/delta) % (delta* 1000000.0/runs)) << endl;
  g_totalRuns += runs;
}

struct ARecordTest
{
  explicit ARecordTest(int records) : d_records(records) {}

  string getName() const
  {
    return (boost::format("%d a records") % d_records).str();
  }

  void operator()() const
  {
    vector<uint8_t> packet;
    DNSPacketWriter pw(packet, DNSName("outpost.ds9a.nl"), QType::A);
    for(int records = 0; records < d_records; records++) {
      pw.startRecord(DNSName("outpost.ds9a.nl"), QType::A);
      ARecordContent arc("1.2.3.4");
      arc.toPacket(pw);
    }
    pw.commit();
  }
  int d_records;
};


struct MakeStringFromCharStarTest
{
  MakeStringFromCharStarTest() : d_size(0){}
  string getName() const
  {
    return (boost::format("make a std::string")).str();
  }

  void operator()() const
  {
    string name("outpost.ds9a.nl");
    d_size += name.length();
    
  }
  mutable int d_size;
};


struct GetTimeTest
{
  string getName() const
  {
    return "gettimeofday-test";
  }

  void operator()() const
  {
    struct timeval tv;
    gettimeofday(&tv, 0);
  }
};

pthread_mutex_t s_testlock=PTHREAD_MUTEX_INITIALIZER;

struct GetLockUncontendedTest
{
  string getName() const
  {
    return "getlock-uncontended-test";
  }

  void operator()() const
  {
    pthread_mutex_lock(&s_testlock);
    pthread_mutex_unlock(&s_testlock);
  }
};


struct StaticMemberTest
{
  string getName() const
  {
    return "static-member-test";
  }

  void operator()() const
  {
    static string* s_ptr;
    if(!s_ptr)
      s_ptr = new string();
  }
};

struct StringtokTest
{
  string getName() const
  {
    return "stringtok";
  }
  
  void operator()() const 
  {
    string str("the quick brown fox jumped");
    vector<string> parts;
    stringtok(parts, str);
  }
};

struct VStringtokTest
{
  string getName() const
  {
    return "vstringtok";
  }
  
  void operator()() const 
  {
    string str("the quick brown fox jumped");
    vector<pair<unsigned int, unsigned> > parts;
    vstringtok(parts, str);
  }
};

struct StringAppendTest
{
  string getName() const
  {
    return "stringappend";
  }
  
  void operator()() const 
  {
    string str;
    static char i;
    for(int n=0; n < 1000; ++n)
      str.append(1, i);
    i++; 
  }
};


struct MakeARecordTest
{
  string getName() const
  {
    return (boost::format("make a-record")).str();
  }

  void operator()() const
  {
      static string src("1.2.3.4");
      ARecordContent arc(src);
      //ARecordContent arc(0x01020304);

  }
};

struct MakeARecordTestMM
{
  string getName() const
  {
    return (boost::format("make a-record (mm)")).str();
  }

  void operator()() const
  {
      DNSRecordContent*drc = DNSRecordContent::mastermake(QType::A, 1, 
                                                          "1.2.3.4");
      delete drc;
  }
};


struct A2RecordTest
{
  explicit A2RecordTest(int records) : d_records(records) {}

  string getName() const
  {
    return (boost::format("%d a records") % d_records).str();
  }

  void operator()() const
  {
    vector<uint8_t> packet;
    DNSPacketWriter pw(packet, DNSName("outpost.ds9a.nl"), QType::A);
    ARecordContent arc("1.2.3.4");
    DNSName name("outpost.ds9a.nl");
    for(int records = 0; records < d_records; records++) {
      pw.startRecord(name, QType::A);

      arc.toPacket(pw);
    }
    pw.commit();
  }
  int d_records;
};


struct TXTRecordTest
{
  explicit TXTRecordTest(int records) : d_records(records) {}

  string getName() const
  {
    return (boost::format("%d TXT records") % d_records).str();
  }

  void operator()() const
  {
    vector<uint8_t> packet;
    DNSPacketWriter pw(packet, DNSName("outpost.ds9a.nl"), QType::TXT);
    for(int records = 0; records < d_records; records++) {
      pw.startRecord(DNSName("outpost.ds9a.nl"), QType::TXT);
      TXTRecordContent arc("\"een leuk verhaaltje in een TXT\"");
      arc.toPacket(pw);
    }
    pw.commit();
  }
  int d_records;
};


struct GenericRecordTest
{
  explicit GenericRecordTest(int records, uint16_t type, const std::string& content) 
    : d_records(records), d_type(type), d_content(content) {}

  string getName() const
  {
    return (boost::format("%d %s records") % d_records % 
            DNSRecordContent::NumberToType(d_type)).str();
  }

  void operator()() const
  {
    vector<uint8_t> packet;
    DNSPacketWriter pw(packet, DNSName("outpost.ds9a.nl"), d_type);
    for(int records = 0; records < d_records; records++) {
      pw.startRecord(DNSName("outpost.ds9a.nl"), d_type);
      DNSRecordContent*drc = DNSRecordContent::mastermake(d_type, 1, 
                                                          d_content);
      drc->toPacket(pw);
      delete drc;
    }
    pw.commit();
  }
  int d_records;
  uint16_t d_type;
  string d_content;
};


struct AAAARecordTest
{
  explicit AAAARecordTest(int records) : d_records(records) {}

  string getName() const
  {
    return (boost::format("%d aaaa records (mm)") % d_records).str();
  }

  void operator()() const
  {
    vector<uint8_t> packet;
    DNSPacketWriter pw(packet, DNSName("outpost.ds9a.nl"), QType::AAAA);
    for(int records = 0; records < d_records; records++) {
      pw.startRecord(DNSName("outpost.ds9a.nl"), QType::AAAA);
      DNSRecordContent*drc = DNSRecordContent::mastermake(QType::AAAA, 1, "fe80::21d:92ff:fe6d:8441");
      drc->toPacket(pw);
      delete drc;
    }
    pw.commit();
  }
  int d_records;
};

struct SOARecordTest
{
  explicit SOARecordTest(int records) : d_records(records) {}

  string getName() const
  {
    return (boost::format("%d soa records (mm)") % d_records).str();
  }

  void operator()() const
  {
    vector<uint8_t> packet;
    DNSPacketWriter pw(packet, DNSName("outpost.ds9a.nl"), QType::SOA);

    for(int records = 0; records < d_records; records++) {
      pw.startRecord(DNSName("outpost.ds9a.nl"), QType::SOA);
      DNSRecordContent*drc = DNSRecordContent::mastermake(QType::SOA, 1, "a0.org.afilias-nst.info. noc.afilias-nst.info. 2008758137 1800 900 604800 86400");
      drc->toPacket(pw);
      delete drc;
    }
    pw.commit();
  }
  int d_records;
};

vector<uint8_t> makeEmptyQuery()
{
  vector<uint8_t> packet;
  DNSPacketWriter pw(packet, DNSName("outpost.ds9a.nl"), QType::SOA);
  return  packet;
}

vector<uint8_t> makeTypicalReferral()
{
  vector<uint8_t> packet;
  DNSPacketWriter pw(packet, DNSName("outpost.ds9a.nl"), QType::A);

  pw.startRecord(DNSName("ds9a.nl"), QType::NS, 3600, 1, DNSResourceRecord::AUTHORITY);
  DNSRecordContent* drc = DNSRecordContent::mastermake(QType::NS, 1, "ns1.ds9a.nl");
  drc->toPacket(pw);
  delete drc;

  pw.startRecord(DNSName("ds9a.nl"), QType::NS, 3600, 1, DNSResourceRecord::AUTHORITY);
  drc = DNSRecordContent::mastermake(QType::NS, 1, "ns2.ds9a.nl");
  drc->toPacket(pw);
  delete drc;


  pw.startRecord(DNSName("ns1.ds9a.nl"), QType::A, 3600, 1, DNSResourceRecord::ADDITIONAL);
  drc = DNSRecordContent::mastermake(QType::A, 1, "1.2.3.4");
  drc->toPacket(pw);
  delete drc;

  pw.startRecord(DNSName("ns2.ds9a.nl"), QType::A, 3600, 1, DNSResourceRecord::ADDITIONAL);
  drc = DNSRecordContent::mastermake(QType::A, 1, "4.3.2.1");
  drc->toPacket(pw);
  delete drc;

  pw.commit();
  return  packet;
}

struct StackMallocTest
{
  string getName() const
  {
    return "stack allocation";
  }

  void operator()() const
  {
    char *buffer= new char[200000];
    delete[] buffer;
  }

};


struct EmptyQueryTest
{
  string getName() const
  {
    return "write empty query";
  }

  void operator()() const
  {
    vector<uint8_t> packet=makeEmptyQuery();
  }

};

struct TypicalRefTest
{
  string getName() const
  {
    return "write typical referral";
  }

  void operator()() const
  {
    vector<uint8_t> packet=makeTypicalReferral();
  }

};

struct TCacheComp
{
  bool operator()(const pair<string, QType>& a, const pair<string, QType>& b) const
  {
    int cmp=strcasecmp(a.first.c_str(), b.first.c_str());
    if(cmp < 0)
      return true;
    if(cmp > 0)
      return false;

    return a.second < b.second;
  }
};

struct NegCacheEntry
{
  DNSName d_name;
  QType d_qtype;
  DNSName d_qname;
  uint32_t d_ttd;
};

struct timeval d_now;



struct ParsePacketTest
{
  explicit ParsePacketTest(const vector<uint8_t>& packet, const std::string& name) 
    : d_packet(packet), d_name(name)
  {}

  string getName() const
  {
    return "parse '"+d_name+"'";
  }

  void operator()() const
  {
    MOADNSParser mdp(false, (const char*)&*d_packet.begin(), d_packet.size());
    
    struct {
            vector<DNSResourceRecord> d_result;
            bool d_aabit;
            int d_rcode;
    } lwr;
    DNSResourceRecord rr;
    for(MOADNSParser::answers_t::const_iterator i=mdp.d_answers.begin(); i!=mdp.d_answers.end(); ++i) {          
      DNSResourceRecord rr;
      rr.qtype=i->first.d_type;
      rr.qname=i->first.d_name;
    
      rr.ttl=i->first.d_ttl;
      rr.content=i->first.d_content->getZoneRepresentation();  // this should be the serialised form
      rr.d_place=(DNSResourceRecord::Place) i->first.d_place;
      lwr.d_result.push_back(rr);
    }


  }
  const vector<uint8_t>& d_packet;
  std::string d_name;
};

struct ParsePacketBareTest
{
  explicit ParsePacketBareTest(const vector<uint8_t>& packet, const std::string& name) 
    : d_packet(packet), d_name(name)
  {}

  string getName() const
  {
    return "parse '"+d_name+"' bare";
  }

  void operator()() const
  {
    MOADNSParser mdp(false, (const char*)&*d_packet.begin(), d_packet.size());
  }
  const vector<uint8_t>& d_packet;
  std::string d_name;
};


struct SimpleCompressTest
{
  explicit SimpleCompressTest(const std::string& name) 
    : d_name(name)
  {}

  string getName() const
  {
    return "compress '"+d_name+"'";
  }

  void operator()() const
  {
    simpleCompress(d_name);
  }
  std::string d_name;
};

struct VectorExpandTest
{
  string getName() const
  {
    return "vector expand";
  }

  void operator()() const
  {
    vector<uint8_t> d_record;
    d_record.resize(12);

    string out="\x03www\x04ds9a\x02nl";
    string::size_type len = d_record.size();
    d_record.resize(len + out.length());
    memcpy(&d_record[len], out.c_str(), out.length());
  }

};

struct DNSNameParseTest
{
  string getName() const
  {
    return "DNSName parse";
  }

  void operator()() const
  {
    DNSName name("www.powerdns.com");
  }

};

struct DNSNameRootTest
{
  string getName() const
  {
    return "DNSName root";
  }

  void operator()() const
  {
    DNSName name(".");
  }

};



struct IEqualsTest
{
  string getName() const
  {
    return "iequals test";
  }

  void operator()() const
  {
      static string a("www.ds9a.nl"), b("www.lwn.net");
      g_ret = boost::iequals(a, b);
  }

};

struct MyIEqualsTest
{
  string getName() const
  {
    return "pdns_iequals test";
  }

  void operator()() const
  {
      static string a("www.ds9a.nl"), b("www.lwn.net");
      g_ret = pdns_iequals(a, b);
  }

};


struct StrcasecmpTest
{
  string getName() const
  {
    return "strcasecmp test";
  }

  void operator()() const
  {
      static string a("www.ds9a.nl"), b("www.lwn.net");
      g_ret = strcasecmp(a.c_str(), b.c_str());
  }
};


struct Base64EncodeTest
{
  string getName() const
  {
    return "Bas64Encode test";
  }

  void operator()() const
  {
      static string a("dq4KydZjmcoQQ45VYBP2EDR8FqKaMul0eSHBt7Xx5F7A4HFtabXEzDLD01bnSiGK");
      Base64Encode(a);
  }
};


struct B64DecodeTest
{
  string getName() const
  {
    return "B64Decode test";
  }

  void operator()() const
  {
      static string a("ZHE0S3lkWmptY29RUTQ1VllCUDJFRFI4RnFLYU11bDBlU0hCdDdYeDVGN0E0SEZ0YWJYRXpETEQwMWJuU2lHSw=="), b;
      g_ret = B64Decode(a,b);
  }
};


struct NOPTest
{
  string getName() const
  {
    return "null test";
  }

  void operator()() const
  {
  }

};



int main(int argc, char** argv)
try
{
  reportAllTypes();

  doRun(NOPTest());

  doRun(IEqualsTest());
  doRun(MyIEqualsTest());
  doRun(StrcasecmpTest());
  doRun(Base64EncodeTest());
  doRun(B64DecodeTest());

  doRun(StackMallocTest());

  doRun(EmptyQueryTest());
  doRun(TypicalRefTest());


  auto packet = makeEmptyQuery();
  doRun(ParsePacketTest(packet, "empty-query"));

  packet = makeTypicalReferral();
  cerr<<"typical referral size: "<<packet.size()<<endl;
  doRun(ParsePacketBareTest(packet, "typical-referral"));

  doRun(ParsePacketTest(packet, "typical-referral"));

  doRun(SimpleCompressTest("www.france.ds9a.nl"));

  
  doRun(VectorExpandTest());

  doRun(GetTimeTest());
  
  doRun(GetLockUncontendedTest());
  doRun(StaticMemberTest());
  
  doRun(ARecordTest(1));
  doRun(ARecordTest(2));
  doRun(ARecordTest(4));
  doRun(ARecordTest(64));

  doRun(A2RecordTest(1));
  doRun(A2RecordTest(2));
  doRun(A2RecordTest(4));
  doRun(A2RecordTest(64));

  doRun(MakeStringFromCharStarTest());
  doRun(MakeARecordTest());
  doRun(MakeARecordTestMM());

  doRun(AAAARecordTest(1));
  doRun(AAAARecordTest(2));
  doRun(AAAARecordTest(4));
  doRun(AAAARecordTest(64));

  doRun(TXTRecordTest(1));
  doRun(TXTRecordTest(2));
  doRun(TXTRecordTest(4));
  doRun(TXTRecordTest(64));

  doRun(GenericRecordTest(1, QType::NS, "powerdnssec1.ds9a.nl"));
  doRun(GenericRecordTest(2, QType::NS, "powerdnssec1.ds9a.nl"));
  doRun(GenericRecordTest(4, QType::NS, "powerdnssec1.ds9a.nl"));
  doRun(GenericRecordTest(64, QType::NS, "powerdnssec1.ds9a.nl"));

  

  doRun(SOARecordTest(1));
  doRun(SOARecordTest(2));
  doRun(SOARecordTest(4));
  doRun(SOARecordTest(64));

  doRun(StringtokTest());
  doRun(VStringtokTest());  
  doRun(StringAppendTest());  

  doRun(DNSNameParseTest());
  doRun(DNSNameRootTest());

  cerr<<"Total runs: " << g_totalRuns<<endl;

}
catch(std::exception &e)
{
  cerr<<"Fatal: "<<e.what()<<endl;
}

