#pragma once

#include "messenger.h"
#include "acceptor.h"
#include "proposalid.h"
#include "proposer.h"
#include "learner.h"

#include "sys/util.h"

class PaxosNode
{
public:
	PaxosNode(const Messenger& messenger, const std::string& proposerUID, int quorumSize);
	~PaxosNode();

	//节点paxos协议功能部分
	bool isActive();
	void setActive(bool active);
	//learner
	bool isComplete(); 
	void receiveAccepted(const std::string& fromUID, const ProposalID& proposalID, const std::string& acceptedValue);
	std::string getFinalValue();
	ProposalID getFinalProposalID();
	//acceptor
	void receivePrepare(const std::string& fromUID, const ProposalID& proposalID);
	void receiveAcceptRequest(const std::string& fromUID, const ProposalID& proposalID, const std::string& value);
	ProposalID getPromisedID();
	ProposalID getAcceptedID(); 
	std::string getAcceptedValue();
	bool persistenceRequired(); 
	void recover(const ProposalID& promisedID, const ProposalID& acceptedID, const std::string& acceptedValue);
	void persisted();
	//proposer
	void setProposal(const std::string& value);
	virtual void prepare();
	virtual void prepare(bool incrementProposalNumber);
	void receivePromise(const std::string& fromUID, const ProposalID& proposalID, const ProposalID& prevAcceptedID, const std::string& prevAcceptedValue); 
	Messenger getMessenger();
	std::string getProposerUID();
	int getQuorumSize();
	ProposalID getProposalID();
	std::string getProposedValue();
	ProposalID getLastAcceptedID();
	int numPromises();
	void observeProposal(const std::string& fromUID, const ProposalID& proposalID);
	void receivePrepareNACK(const std::string& proposerUID, const ProposalID& proposalID, const ProposalID& promisedID);
	void receiveAcceptNACK(const std::string& proposerUID, const ProposalID& proposalID, const ProposalID& promisedID);
	void resendAccept();
	bool isLeader();
	void setLeader(bool leader);

private:
	//节点可以同时具备Proposer、Acceptor、Learner三种系统角色的功能
	Proposer* m_proposerPtr;
	Acceptor* m_acceptorPtr;
	Learner*  m_learnerPtr;
};

class HeartbeatNode : public PaxosNode
{
public:
	HeartbeatNode(const Messenger& messenger, const std::string& proposerUID, int quorumSize, const std::string& leaderUID, int heartbeatPeriod, int livenessWindow);
	~HeartbeatNode(){}

	//节点心跳功能部分
	long timestamp();
	std::string getLeaderUID();
	ProposalID getLeaderProposalID();
	void setLeaderProposalID( const ProposalID& newLeaderID ); 
	bool isAcquiringLeadership();
	void prepare(bool incrementProposalNumber);
	bool leaderIsAlive();
	bool observedRecentPrepare();
	void pollLiveness();
	void receiveHeartbeat(const std::string& fromUID, const ProposalID& proposalID); 
	void pulse();
	void acquireLeadership();
	void receivePrepare(const std::string& fromUID, const ProposalID& proposalID);
	void receivePromise(const std::string& fromUID, const ProposalID& proposalID, const ProposalID& prevAcceptedID, const std::string& prevAcceptedValue);
	void receivePrepareNACK(const std::string& proposerUID, const ProposalID& proposalID, const ProposalID& promisedID);
	void receiveAcceptNACK(const std::string& proposerUID, const ProposalID& proposalID, const ProposalID& promisedID);
private:
	//节点心跳功能部分
	std::string	m_leaderUID;
	ProposalID	m_leaderProposalID;
	long	m_lastHeartbeatTimestamp;
	long	m_lastPrepareTimestamp;
	long	m_heartbeatPeriod         = 1000; // Milliseconds
	long	m_livenessWindow          = 5000; // Milliseconds
	bool	m_acquiringLeadership     = false;
	std::set<std::string>	m_acceptNACKs;

	Messenger m_messenger;
};