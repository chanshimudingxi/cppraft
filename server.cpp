#include "server.h"
#include <sstream>

Server::Server(){
	m_container = new EpollContainer(1000, 1000);
	assert(nullptr != m_container);
	m_commonProtoParser = new CommonProtoParser();
	assert(nullptr != m_commonProtoParser);
	m_commonProtoParser->SetCallback(HandleMessage, this);
}

Server::~Server(){
	if(nullptr != m_container){
		delete m_container;
	}
	if(nullptr != m_commonProtoParser){
		delete m_commonProtoParser;
	}
}

bool Server::Init(const std::string& localIP, uint16_t localPort, const std::string& dstIP, uint16_t dstPort){
    if(!m_container->Init()){
        return false;
    }
	m_localIP = inet_addr(localIP.c_str());
	m_localPort = localPort;
	m_dstIP = inet_addr(dstIP.c_str());
	m_dstPort = dstPort;
	return true;
}

bool Server::Listen(int port, int backlog, SocketType type){
	switch(type){
		case SocketType::tcp:{
			return TcpSocket::Listen(port, backlog, m_container, m_commonProtoParser);
		}
		break;
		case SocketType::udp:{
			return UdpSocket::Listen(port, backlog, m_container, m_commonProtoParser);
		}
		break;
		default:{
			return false;
		}
		break;
	}
	return true;
}

bool Server::Run(){
	if(!Listen(m_localPort, 10, SocketType::tcp)){
		return false;
	}
	if(!Listen(m_localPort, 10, SocketType::udp)){
		return false;
	}

	while(true){
		uint64_t start = Util::GetMonoTimeUs();
		m_container->HandleSockets();
		TimerCheck();
		uint64_t end = Util::GetMonoTimeUs();
		LOG_DEBUG("loop once cost:%lu", end - start);
    }
}

bool Server::HandleMessage(std::shared_ptr<Message> pMsg, SocketBase* s, void* instance){
	if(instance != nullptr){
		return static_cast<Server*>(instance)->HandleMessage(pMsg, s);
	}
	return false;
}

bool Server::HandleMessage(std::shared_ptr<Message> pMsg, SocketBase* s){
	switch (pMsg->ProtoID()){
		case PAXOS_PROTO_PING_MESSAGE:
			return HandlePingMessage(std::dynamic_pointer_cast<PingMessage>(pMsg), s);
		default:
			return false;
	}
}

bool Server::HandlePingMessage(std::shared_ptr<PingMessage> pMsg, SocketBase* s){
	uint64_t lastStamp = pMsg->m_stamp;
	std::string peerId = pMsg->m_myInfo.m_id;

	Peer peer;
	peer.m_id = peerId;
	std::stringstream os;
	for(int i = 0; i< pMsg->m_myInfo.m_addrs.size(); ++i){
		ProtoPeerAddr& addr = pMsg->m_myInfo.m_addrs[i];
		os<<"ip:"<<Util::UintIP2String(addr.m_ip)<<" port:"<<addr.m_port<<" type:"<<(addr.m_socketType == 0 ? "tcp ": "udp ");
		if(m_peers.find(peerId) == m_peers.end()){
			PeerAddr peeraddr;
			peeraddr.m_ip = addr.m_ip;
			peeraddr.m_port = addr.m_port;
			peeraddr.m_socketType = addr.m_socketType == 0 ? SocketType::tcp : SocketType::udp;
			peer.m_addrs.push_back(peeraddr);
		}
	}

	if(m_peers.find(peerId) == m_peers.end()){
		m_peers[peerId] = peer;
	}
	uint64_t now = Util::GetMonoTimeMs();
	uint64_t rtt = lastStamp > now ? lastStamp - now : 0;
	LOG_DEBUG("peer id:%s rtt:%llums %s", peerId.c_str(), rtt, os.str().c_str());
	return true;
}

void Server::TimerCheck(){
	SendPingMessageToAll();
}

bool Server::Connect(uint32_t ip, int port, SocketType type, int* pfd){
	switch(type){
		case SocketType::tcp:{
			return TcpSocket::Connect(ip, port, m_container, m_commonProtoParser, pfd);
		}
		break;
		case SocketType::udp:{
			return UdpSocket::Connect(ip, port, m_container, m_commonProtoParser, pfd);
		}
		break;
		default:{
			return false;
		}
		break;
	}
	return true;
}

bool Server::SendMessage(const Message& msg, SocketBase* s){
	std::string packet;
	CommonProtoParser::PackMessage(msg, &packet);
	if(!s->SendPacket(packet.data(),packet.size())){
		LOG_ERROR("fd:%d send packet failed", s->GetFd());
		return false;
	}
	return true;
}

bool Server::SendMessageToPeer(const Message& msg, Peer& peer){
	int ret = 0;
	for(int i=0; peer.m_addrs.size(); ++i){
		PeerAddr& addr = peer.m_addrs[i];
		if(addr.m_alive){
			SocketBase* pSock = m_container->GetSocket(addr.m_fd);
			if(nullptr == pSock){
				LOG_ERROR("fd:%d get connect failed", addr.m_fd);
				continue;
			}
			else{
				if(SendMessage(msg, pSock)){
					ret++;
				}
			}
		}
		else{
			if(Connect(addr.m_ip, addr.m_port, addr.m_socketType, &addr.m_fd)){
				addr.m_alive = true;
			}
		}
	}
	return ret > 0;
}

void Server::SendMessageToAll(const Message& msg){
	int ret = 0;
	for(std::map<std::string, Peer>::iterator itr = m_peers.begin(); itr!=m_peers.end(); ++itr){
		Peer& peer = itr->second;
		if(SendMessageToPeer(msg, peer)){
			ret++;
		}
	}
}

void Server::SendPingMessageToAll(){
	PingMessage ping;
	ping.m_stamp = Util::GetMonoTimeMs();
	ping.m_myInfo.m_id = "123456789";
	ProtoPeerAddr addr;
	addr.m_ip= m_dstIP;
	addr.m_port = m_dstPort;
	addr.m_socketType = 0;
	ping.m_myInfo.m_addrs.push_back(addr);
	addr.m_socketType = 1;
	ping.m_myInfo.m_addrs.push_back(addr);

	SendMessageToAll(ping);
}

