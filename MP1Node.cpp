/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);

}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
        // this call will add it to introducer Membership List  
        addMemberToMembershipList(*(int *)&(memberNode->addr.addr), *(short *) &(memberNode->addr.addr[4]), *(short *)&memberNode->heartbeat);           
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;  
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();
//    printMyMembershipList();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size; 

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;         // the message off the queue
    	size = memberNode->mp1q.front().size;       // number of bytes of message in the queue
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }

    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
    // env is memberNode that rec'd the msg

    MessageHdr *outgoingMsg;
    MessageHdr *receivedMsg;
    Address msgFromAddress;   // complete address of node that sent the message. located in "data"
    int msgFromID;            // id of node that sent the message. located in "data"
    short msgFromPort;        // port of the node that sent the message. located in "data"
    long fromHeartbeat;       // heartbeat of the node that sent the message. located in "data"

    int nodeID = *(int *)(&memberNode->addr.addr);                // ID of the node that rec'd messag
    short nodePort = *(short *)(&memberNode->addr.addr[4]);       // port of the node that rec'd message
    long nodeHeartbeat = *(long *)(&memberNode->heartbeat);       //  heartbeat of node that rec'd a message

    size_t msgPosition = 0;    // used to iterate through the "data"

    // break out info in the data
    msgPosition = sizeof(MsgTypes);    // start after the message type
    receivedMsg = (MessageHdr *) malloc(size * sizeof(char));      // need to allocate memory for the rec'd msg before putting data in it
    memcpy((char*)(receivedMsg),data,size); 
    memcpy(&msgFromID, data+sizeof(MessageHdr), sizeof(int));
    memcpy(&msgFromPort, data+sizeof(MessageHdr) + sizeof(short), sizeof(short));
    memcpy(&msgFromAddress, (char *)receivedMsg + msgPosition, sizeof(Address));
    msgPosition += sizeof(Address);
    memcpy(&fromHeartbeat, (long *)receivedMsg + msgPosition, sizeof(long));
    msgPosition += sizeof(long);

   
    int outgoingMsgSize;

    // new node sent request to introducer node to be added to the program
    
    switch(receivedMsg->msgType)
    {
        case(JOINREQ):      // request to introducer node from newNode to be added to group

 //           cout<<"Node ID " << nodeID << " with port " << nodePort << " rec'd a message from id " << msgFromID << " with port " << msgFromPort << endl;
            addMemberToMembershipList(msgFromID, msgFromPort, fromHeartbeat);         // this call will add it to introducer Membership List
            
            sendMembershipList(msgFromAddress);
            
            outgoingMsgSize = sizeof(MessageHdr)+sizeof(Address)+sizeof(long);
            outgoingMsg = (MessageHdr *) malloc(sizeof(outgoingMsgSize));
            outgoingMsg->msgType = JOINREP;
            memcpy((char *)(outgoingMsg+1),&memberNode->addr.addr,sizeof(memberNode->addr.addr));       //copy address of this node to the outgoing msg
            memcpy((char *)(outgoingMsg+1)+1+sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));  // copy heartbeat of this node to the outgoing msg
            emulNet->ENsend(&memberNode->addr, &msgFromAddress, (char *)outgoingMsg, outgoingMsgSize);  // send JOINREP back to node letting know added
            free(outgoingMsg);
            break;

        case(JOINREP):      // rec'd by the new node just added
            cout << "join reply rec'd by node " << nodeID << " from node " << msgFromID << endl;
            addMemberToMembershipList(nodeID, nodePort, nodeHeartbeat);         // this call will add new node to its own Membership List
   //         mergeMyMembershipList();
            memberNode->inGroup = true;

//            #ifdef DEBUGLOG
//                log->LOG(&memberNode->addr, "Added to group");
//            #endif
      
            break;
        case(GOSSIP):       // member rec'd an updated ML to compare to their own
            int tempID;
            short tempPort;
            long tempHB;
            bool addToList;;
//          cout << "MADE IT TO GOSSIP " << endl; 
            // break out ML from the data
            // msgPosition should be at the 1 item in list
            // format id/port/heartbeat, id/port/heartbeat, ....etc
            while (msgPosition < size)
            {
                memcpy(&tempID, data+msgPosition, sizeof(int));
                msgPosition+=sizeof(int);
                memcpy(&tempPort, data+msgPosition, sizeof(short));
                msgPosition+=sizeof(short);
                memcpy(&tempHB, data+msgPosition, sizeof(long));
                msgPosition+=sizeof(long);

                // we have the ID and heartbeat of another list
                // if we dont have that ID in our list, add it
                // if the heartbeat is newer, then update our heartbeat with current time
                vector<MemberListEntry>::iterator thisMemberPosition;
                for(thisMemberPosition = memberNode->memberList.begin();
                    thisMemberPosition != memberNode->memberList.end();
                    thisMemberPosition++)
                {
                    addToList = true;   // set false if ID match found

                    if(tempID == thisMemberPosition->id)
                    {
                        //check heartbeat
                        addToList = false;
                    } 
                }
                if(addToList)   // ID not found in this members list
                {
 //                   cout<<"adding to list " << endl;
                    addMemberToMembershipList(tempID, tempPort, tempHB);
                }
 //           cout << "id " << tempID << " port " << tempPort << " hearbeat " << tempHB << endl;
//            cout << "message position " << msgPosition << " and message size " << size << endl;
            }
            break;
        case(DUMMYLASTMSGTYPE):
            break;
        default:
            return false;
    } 
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

	/*
	 * Your code goes here
	 */
    sendMembershipList();
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}


// ********  MY ADDED FUNCTION ************ //


void MP1Node::addMemberToMembershipList(int id, short port, long heatbeat)
{
    MemberListEntry *newEntry = new MemberListEntry(id, port, heatbeat, (long)par->getcurrtime());
    memberNode->memberList.emplace_back(*newEntry);

    #ifdef DEBUGLOG
        Address newNodeAddress;
        memcpy(&newNodeAddress.addr, &id,sizeof(int));
        memcpy(&newNodeAddress.addr[4], &port, sizeof(short));
        log->logNodeAdd(&memberNode->addr, &newNodeAddress);
    #endif
}


void MP1Node::removeMemberFromMembershipList(int id, short port)
{
    vector<MemberListEntry>::iterator memberPosition;
    for(memberPosition = memberNode->memberList.begin();
        memberPosition != memberNode->memberList.end();
        memberPosition++)
        {
            if((id == memberPosition->id) && (port == memberPosition->port))
                memberNode->memberList.erase(memberPosition);
                break;
        }

    #ifdef DEBUGLOG
        Address eraseNodeAddress;
        memcpy(&eraseNodeAddress.addr, &id,sizeof(int));
        memcpy(&eraseNodeAddress.addr[4], &port, sizeof(short));
        log->logNodeAdd(&memberNode->addr, &eraseNodeAddress);
    #endif
    //memberNode->memberList.erase(memberNode->myPos);

}
// takes a member list and merges it with "this" members List
void MP1Node::mergeMyMembershipList(Member *mergeWithMember)
{
    vector<MemberListEntry>::iterator newMemberPosition;
    vector<MemberListEntry>::iterator thisMemberPosition;
    bool addToList;    
    
    // go through each member in the list that was passed
    // for each member in this list, go through "this" member's list
    // insert it in if missing
    // if ID matches, then update heartbeat if bigger
    for(newMemberPosition = mergeWithMember->memberList.begin();
        newMemberPosition != mergeWithMember->memberList.end();
        newMemberPosition++)
    {
        addToList = true;       // set false if ID match found
        for(thisMemberPosition = memberNode->memberList.begin();
        thisMemberPosition != memberNode->memberList.end();
        thisMemberPosition++)
        {
            //if(newMemberPosition->id != thisMemberPosition->id)     // member ID of passed list does not match this list
            ///    break;
            if(newMemberPosition->id == thisMemberPosition->id)
            {
                //check heartbeat
                addToList = false;
            } 
        }
        if(addToList)
        {
            addMemberToMembershipList(newMemberPosition->id, newMemberPosition->port, newMemberPosition->heartbeat);
        }
    }

   // go through each member of the new list
   // if there is a node(id) I don't have...add it to my list 


   // if there is a node I have, compare the heartbeat
    // if hearbeat is higher than my heartbeat, update my with the heartbeat of mergeWithMember heartbeat and set time to localtime

}

void MP1Node::printMyMembershipList()
{
    vector<MemberListEntry>::iterator memberPosition;
    int nodeID = *(int *)(&memberNode->addr.addr);                // ID of this node list
    cout << "Node " << nodeID << " Membership List" << endl;
    cout << "ID    " << "Heartbeat   " << "Timestamp" << endl;

 
    for(memberPosition = memberNode->memberList.begin();
        memberPosition != memberNode->memberList.end();
        memberPosition++)
        {
           cout<< memberPosition->getid() << "     " << memberPosition->getheartbeat() << "           " << memberPosition->gettimestamp() << endl;
        }
    cout << endl;
    
}

MessageHdr* MP1Node::getMembershipListToSend()
{
    MessageHdr* memberList;
    size_t numMembers = memberNode->memberList.size();
    // create array of this size
    
  

}
void MP1Node::processJoinRequest()
{

    // 
       cout<<"join request rec'd";
}
void MP1Node::processJoinResponseRequest()
{

}

// send member list to defined number of random nodes
void MP1Node::sendMembershipList()
{
    int numMembers = memberNode->memberList.size();

    if(numMembers < 2)
    {
 //       cout << "only " << numMembers;
        return;
    }
        
    int msgPosition = 0;
    MessageHdr *membershipListMsg;
    size_t listMsgSize = sizeof sizeof(MessageHdr) + sizeof(Address) + sizeof(long) + 
        (numMembers)*(sizeof(int)+sizeof(short)+sizeof(long));
    membershipListMsg = (MessageHdr *) malloc(listMsgSize);
    membershipListMsg->msgType = GOSSIP;
    msgPosition += sizeof(MsgTypes);    // move pointer to next position in the message

    vector<MemberListEntry>::iterator memberPosition;
     
     // go through this members list and add each entry to the message
     // format id/port/heartbeat, id/port/heartbeat, ....etc
    for(memberPosition = memberNode->memberList.begin();
        memberPosition != memberNode->memberList.end();
        memberPosition++)
    {
       memcpy((char *)(membershipListMsg)+msgPosition, &memberPosition->id, sizeof(memberPosition->id));
       msgPosition += sizeof(memberPosition->id);
       memcpy((char *)(membershipListMsg)+msgPosition, &memberPosition->port, sizeof(memberPosition->port));
       msgPosition += sizeof(memberPosition->port);
       memcpy((char *)(membershipListMsg)+msgPosition, &memberPosition->heartbeat, sizeof(memberPosition->heartbeat));
       msgPosition += sizeof(memberPosition->heartbeat);
    }
    srand(time(NULL));
    for(int i = 0; i<NUMTOGOSSIP; i++)
    {   //*************************   error at random. only 1 item on list...which bring range to 0 ************************************
    // check to make sure they have at least 2 items on list. maybe need to start with intro member **********************
        Address sendTo;
        int sendToID = rand() % (numMembers-1) + 1; // randomly pick with member ID to sent to
        memcpy(&sendTo.addr, &memberNode->memberList[sendToID].id, sizeof(int));
        memcpy(&sendTo.addr[4], &memberNode->memberList[sendToID].port, sizeof(short));
        emulNet->ENsend(&memberNode->addr, &sendTo, (char *)membershipListMsg, listMsgSize);
    }
}
// called by a member to send it to a particular member
// used by the introducer node to send current membership list to recently added node
void MP1Node::sendMembershipList(Address sendToMember)
{ 
 //   int nodeID = *(int *)(&memberNode->addr.addr);  
    cout << "ML sent to " << *(int *)(&sendToMember.addr) << " from " << *(int *)&memberNode->addr.addr << endl;
    int numMembers = memberNode->memberList.size();

    int msgPosition = 0;
    MessageHdr *membershipListMsg;
    size_t listMsgSize = sizeof sizeof(MessageHdr) + sizeof(Address) + sizeof(long) + 
        (numMembers)*(sizeof(int)+sizeof(short)+sizeof(long));
    
    cout << "ML msg size " << listMsgSize << endl;
    membershipListMsg = (MessageHdr *) malloc(listMsgSize);
    membershipListMsg->msgType = GOSSIP;
    msgPosition += sizeof(MsgTypes);    // move pointer to next position in the message
    
    vector<MemberListEntry>::iterator memberPosition;
     
     // go through this members list and add each entry to the message
    for(memberPosition = memberNode->memberList.begin();
        memberPosition != memberNode->memberList.end();
        memberPosition++)
    {
       memcpy((char *)(membershipListMsg)+msgPosition, &memberPosition->id, sizeof(memberPosition->id));
       msgPosition += sizeof(memberPosition->id);
       memcpy((char *)(membershipListMsg)+msgPosition, &memberPosition->port, sizeof(memberPosition->port));
       msgPosition += sizeof(memberPosition->port);
       memcpy((char *)(membershipListMsg)+msgPosition, &memberPosition->heartbeat, sizeof(memberPosition->heartbeat));
       msgPosition += sizeof(memberPosition->heartbeat);
    }


  //      memcpy(&sendTo.addr, &memberNode->memberList[sendToID].id, sizeof(int));
  //      memcpy(&sendTo.addr[4], &memberNode->memberList[sendToID].port, sizeof(short));
    emulNet->ENsend(&memberNode->addr, &sendToMember, (char *)membershipListMsg, listMsgSize);
}

/*
void MP1Node::practiceListMsg()
{
        int numMembers = memberNode->memberList.size();
        MemberListEntry <numMembers>;
        array<MemberListEntry, 10> membersList;
        array <int,8> stuff = {1,2,3,4,5,6,7,8};
        cout << stuff[2] << endl;
        Address joinaddr = getJoinAddress();
        MessageHdr *msg;
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr.addr) + sizeof(long) + 1;  
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = GOSSIP;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

    emulNet->ENsend(&memberNode->addr, &joinaddr, (char *)msg, msgsize);
    return;
}*/

// need to deserialize ML and merge with current ML

