#!/usr/bin/env python3
"""Independent black-box DNS wire and dig checks. Usage: test_dns_smoke.py /path/to/bsty-fastseed"""
import pathlib, socket, struct, subprocess, sys, tempfile, time

def name(s):
    return b''.join(bytes([len(x)])+x.encode() for x in s.split('.'))+b'\0'

def question(kind=1, zone='seeder.globalboost.info', edns=True, size=1232, version=0, do=False):
    q=struct.pack('!6H',0x1234,0x100,1,0,0,int(edns))+name(zone)+struct.pack('!HH',kind,1)
    if edns:q+=b'\0'+struct.pack('!HHIH',41,size,(version<<16)|(0x8000 if do else 0),0)
    return q

def read_name(data,pos,seen=None):
    seen=set() if seen is None else seen
    labels=[]
    while True:
        n=data[pos];pos+=1
        if not n:return '.'.join(labels),pos
        if n&0xc0==0xc0:
            off=((n&63)<<8)|data[pos];pos+=1
            assert off not in seen
            seen.add(off);tail,_=read_name(data,off,seen)
            labels.append(tail);return '.'.join(labels),pos
        assert n<=63
        labels.append(data[pos:pos+n].decode());pos+=n

def decode(data):
    ident,flags,qd,an,ns,ar=struct.unpack_from('!6H',data)
    p=12
    for _ in range(qd):_,p=read_name(data,p);p+=4
    sections=[]
    for count in (an,ns,ar):
        rows=[]
        for _ in range(count):
            owner,p=read_name(data,p);kind,klass,ttl,n=struct.unpack_from('!HHIH',data,p);p+=10
            start=p;p+=n
            assert p<=len(data)
            rows.append((owner,kind,klass,ttl,data[start:p],start))
        sections.append(rows)
    assert p==len(data)
    return flags,sections

def receive_exact(sock,n):
    data=b''
    while len(data)<n:
        part=sock.recv(n-len(data));assert part;data+=part
    return data

def main():
    binary=str(pathlib.Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix='fastseed-dns-') as temp:
        port_socket=socket.socket();port_socket.bind(('127.0.0.1',0));port=port_socket.getsockname()[1];port_socket.close()
        cfg=pathlib.Path(temp)/'test.toml'
        cfg.write_text(f'''[dns]
zone = "seeder.globalboost.info"
listen_address = "127.0.0.1"
listen_port = {port}
nameserver = "ns1.example.org"
soa_mailbox = "hostmaster.example.org"
soa_serial = 2026102201
[bootstrap]
dns_seeds = ["127.0.0.1"]
[database]
path = "{temp}/peers.db"
''')
        subprocess.run([binary,'--check-config',str(cfg)],check=True)
        proc=subprocess.Popen([binary,'run','--config',str(cfg)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        try:
            for _ in range(100):
                if proc.poll() is not None:raise RuntimeError(proc.communicate())
                try:
                    with socket.create_connection(('127.0.0.1',port),timeout=.2):pass
                    break
                except OSError:time.sleep(.05)
            else:raise RuntimeError('listener did not start')
            count=0
            for tcp in (False,True):
                for kind in (1,28,2,6,16):
                    for edns in (False,True):
                        request=question(kind,edns=edns,do=True)
                        with socket.socket(type=socket.SOCK_STREAM if tcp else socket.SOCK_DGRAM) as sock:
                            sock.settimeout(3);sock.connect(('127.0.0.1',port))
                            if tcp:
                                sock.sendall(struct.pack('!H',len(request))+request);data=receive_exact(sock,struct.unpack('!H',receive_exact(sock,2))[0])
                            else:sock.send(request);data=sock.recv(65535)
                        flags,sections=decode(data);answer,authority,additional=sections
                        assert flags==0x8500,(tcp,kind,edns,hex(flags))
                        assert len(additional)==int(edns)
                        if edns:assert additional[0][:4]==('',41,1232,0x8000)
                        if kind in (2,6):assert len(answer)==1 and not authority;row=answer[0]
                        else:assert not answer and len(authority)==1;row=authority[0]
                        assert row[0]=='seeder.globalboost.info'
                        if row[1]==2:assert read_name(data,row[5])[0]=='ns1.example.org'
                        else:
                            assert row[1]==6
                            mname,p=read_name(data,row[5]);mailbox,p=read_name(data,p)
                            assert (mname,mailbox)==('ns1.example.org','hostmaster.example.org')
                            assert struct.unpack_from('!5I',data,p)==(2026102201,3600,600,86400,60)
                        count+=1
            print(f'Independent wire decoder: {count} UDP/TCP type/EDNS combinations PASS',flush=True)
            cases=[['A'],['+noedns','A'],['+tcp','A'],['AAAA'],['SOA'],['NS'],['+edns=1','+noednsnegotiation','A'],['+bufsize=512','+ignore','AAAA']]
            for args in cases:
                result=subprocess.run(['dig','@127.0.0.1','-p',str(port),'seeder.globalboost.info','+time=2','+tries=1']+args,text=True,capture_output=True,check=True)
                expected='BADVERS' if '+edns=1' in args else 'NOERROR'
                assert f'status: {expected}' in result.stdout,result.stdout
                assert ' aa' in result.stdout or expected=='BADVERS'
                print('DIG '+' '.join(args)+' PASS\n'+result.stdout,flush=True)
            print('Black-box application smoke: PASS')
        finally:
            proc.terminate()
            try:proc.communicate(timeout=5)
            except subprocess.TimeoutExpired:proc.kill();proc.communicate()
if __name__=='__main__':main()

