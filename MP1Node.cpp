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
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    memberNode->heartbeat = 5;


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
        msg->msgType = JOINREQ;

        memcpy((char *)(msg)+sizeof(MessageHdr), &memberNode->addr.addr, sizeof(Address));
        memcpy((char *)(msg)+sizeof(MessageHdr) + sizeof(Address), &memberNode->heartbeat, sizeof(long));
 //       memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
 //       memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

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
    long nodeHeartbeat = *(long *)(&memberNode->heartbeat);       //  heartbeat of node that rec'd a message  *** HB is accurate

    size_t msgPosition = 0;    // used to iterate through the "data"

    // break out info in the data
    msgPosition += sizeof(MsgTypes);    // start after the message type
    receivedMsg = (MessageHdr *) malloc(size * sizeof(char));      // need to allocate memory for the rec'd msg before putting data in it
    memcpy((char*)(receivedMsg),data,size); 
    memcpy(&msgFromID, data+sizeof(MessageHdr), sizeof(int));

    memcpy(&msgFromPort, data+sizeof(MessageHdr) + sizeof(short), sizeof(short));
    memcpy(&fromHeartbeat, data+sizeof(MessageHdr)+sizeof(Address), sizeof(long));
    memcpy(&msgFromAddress, (char *)receivedMsg + msgPosition, sizeof(Address));
    msgPosition += sizeof(Address);
  //  memcpy(&fromHeartbeat, (long *)receivedMsg + msgPosition, sizeof(long));  // ******************* NOT CORRECT ****************

    msgPosition += sizeof(long);

   
    int outgoingMsgSize;

    // new node sent request to introducer node to be added to the program
    
    switch(receivedMsg->msgType)
    {
        case(JOINREQ):      // request to introducer node from newNode to be added to group
            addMemberToMembershipList(msgFromID, msgFromPort, fromHeartbeat);         // this call will add it to introducer Membership List
            sendMembershipList(msgFromAddress);     // give new node the current membership list
            
            outgoingMsgSize = sizeof(MessageHdr)+sizeof(Address)+sizeof(long);
            outgoingMsg = (MessageHdr *) malloc(sizeof(outgoingMsgSize));
            outgoingMsg->msgType = JOINREP;
            memcpy((char *)(outgoingMsg)+sizeof(MessageHdr),&memberNode->addr.addr,sizeof(Address));       //copy address of this node to the outgoing msg
            memcpy((char *)(outgoingMsg)+sizeof(MessageHdr)+sizeof(Address), &memberNode->heartbeat, sizeof(long));  // copy heartbeat of this node to the outgoing msg
            emulNet->ENsend(&memberNode->addr, &msgFromAddress, (char *)outgoingMsg, outgoingMsgSize);  // send JOINREP back to node letting know added
            free(outgoingMsg);
            break;

        case(JOINREP):      // rec'd by the new node just added
 //           cout << "join reply rec'd by node " << nodeID << " with heartbeat " << nodeHeartbeat << endl;
            //addMemberToMembershipList(nodeID, nodePort, nodeHeartbeat);         // this call will add new node to its own Membership List
            memberNode->inGroup = true;
//            memberNode->heartbeat +=1;

            #ifdef DEBUGLOG
 //              log->LOG(&memberNode->addr, "Added to group");
            #endif
      
            break;
        case(GOSSIP):       // member rec'd an updated ML to compare to their own
            int tempID;
            short tempPort;
            long tempHB;
            bool alreadyInList;
            int numMembers;
            memcpy(&numMembers, data+msgPosition, sizeof(int));
            msgPosition+=sizeof(int);

//            if(nodeID == 1)
//            {
//            cout << "ML rec'd by " << nodeID << " from " << msgFromID << endl; 
//            }

    
            // format id/port/heartbeat, id/port/heartbeat, ....etc
            while (numMembers > 0)
            {
                numMembers--;
                memcpy(&tempID, data+msgPosition, sizeof(int));
                msgPosition+=sizeof(int);
                memcpy(&tempPort, data+msgPosition, sizeof(short));
                msgPosition+=sizeof(short);
                memcpy(&tempHB, data+msgPosition, sizeof(long));
                msgPosition+=sizeof(long);
          //      cout << "checking to see if temp ID: " << tempID << " is in members " << (int)memberNode->addr.addr[0] << endl;
                vector<MemberListEntry>::iterator thisMemberPosition;

                alreadyInList = false;
                for(thisMemberPosition = memberNode->memberList.begin();
                    thisMemberPosition != memberNode->memberList.end();
                    thisMemberPosition++)
                {  
                    if(tempID == thisMemberPosition->id)        // already have this in my list. need to check HB
                    {
                        if(thisMemberPosition->heartbeat < tempHB)
                        {
                            thisMemberPosition->heartbeat = tempHB;
                            thisMemberPosition->timestamp = par->getcurrtime();
                        }
                        alreadyInList = true;
                    } 
                }
                if(!alreadyInList)   // ID not found in this members list
                {
                    addMemberToMembershipList(tempID, tempPort, tempHB);
                }
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

    if (par->getcurrtime() % GOSSIPTIME != 0) {
	    return;
    }
// implement memberFail
// if enough time has gone by...declare member as failed
// send msg fail to all other members

 vector<MemberListEntry>::iterator memberPosition;
     
    int tempPosition = 0;
    
    int tempID;
    int tempTime;
    short tempPort;
    int nodeID = *(int *)(&memberNode->addr.addr); 
    int numMembers = memberNode->memberList.size();

    // go through each item in member list. if heartbeat has not been upated  for x number of time, declare member as suspect
    // after Tsuspect time, if still no update, declare as failed
    for(memberPosition = memberNode->memberList.begin();
        memberPosition != memberNode->memberList.end();
        memberPosition++)
    {
        tempID = memberPosition->id;
        tempPort = memberPosition->port;
        tempTime = memberPosition->timestamp;

       // if after TFAIL time, if heartbeat has not increased, then declared as failed
        if(tempTime + TREMOVE < par->getcurrtime())       // possible failure
        {
//            cout << "node " << tempID << " has failed at " << tempTime << " detected by " << nodeID << endl;
            removeMemberFromMembershipList(tempID, tempPort);
        }

    }
  
    int listPosition = getListPositionByAddress(memberNode->addr);
    if(listPosition < 0)
    {
        cout << "ML posistion: " << listPosition << " does not exist" << endl;
        return;
    }
    memberNode->heartbeat +=1;
    memberNode->memberList[listPosition].heartbeat = memberNode->heartbeat;
    memberNode->memberList[listPosition].timestamp = par->getcurrtime();

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
            {
                memberNode->memberList.erase(memberPosition);
                break;
            }
        }

    #ifdef DEBUGLOG
        Address eraseNodeAddress;
        memcpy(&eraseNodeAddress.addr, &id,sizeof(int));
        memcpy(&eraseNodeAddress.addr[4], &port, sizeof(short));
        log->logNodeRemove(&memberNode->addr, &eraseNodeAddress);
    #endif
    //memberNode->memberList.erase(memberNode->myPos);
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
void MP1Node::printMemberData()
{
    cout << " Member info::: ID " << memberNode->addr.addr[0] << " HEARTBEAT " << memberNode->heartbeat << endl;
}

MessageHdr* MP1Node::getMembershipListToSend()
{

}
void MP1Node::processJoinRequest()
{

}
void MP1Node::processJoinResponseRequest()
{

}

// called by a member to send it to a particular member
// used by the introducer node to send current membership list to recently added node
void MP1Node::sendMembershipList(Address sendToMember)
{
    int tempID;
    short tempPort;
    long tempHB; 
    int nodeID = *(int *)(&memberNode->addr.addr);  
    int numMembers = memberNode->memberList.size();     // will be added to message to let receiver know how many members their are
    int msgPosition = 0;
    MessageHdr *membershipListMsg;
    size_t listMsgSize = sizeof sizeof(MessageHdr) + sizeof(Address) + sizeof(long) + sizeof(int) +
        (numMembers)*(sizeof(int)+sizeof(short)+sizeof(long));

    membershipListMsg = (MessageHdr *) malloc(listMsgSize);
    membershipListMsg->msgType = GOSSIP;
    msgPosition += sizeof(MsgTypes);    // move pointer to next position in the message
    memcpy((char *)(membershipListMsg)+msgPosition,&memberNode->addr.addr,sizeof(Address));
    msgPosition += sizeof(Address);
    memcpy((char *)(membershipListMsg) + msgPosition, &memberNode->heartbeat, sizeof(long));
    msgPosition += sizeof(long);
    memcpy((char *)(membershipListMsg) + msgPosition, &numMembers, sizeof(int));        // pass number of members in the list
    msgPosition += sizeof(int);
    
    vector<MemberListEntry>::iterator memberPosition;
     
     // go through this members list and add each entry to the message
    int tempPosition = 0;
    for(memberPosition = memberNode->memberList.begin();
        memberPosition != memberNode->memberList.end();
        memberPosition++)
    {  

        memcpy((char *)(membershipListMsg)+msgPosition, &memberPosition->id, sizeof(int));
        msgPosition += sizeof(int);
        memcpy((char *)(membershipListMsg)+msgPosition, &memberPosition->port, sizeof(short));
        msgPosition += sizeof(short);
        memcpy((char *)(membershipListMsg)+msgPosition, &memberPosition->heartbeat, sizeof(long));
        msgPosition += sizeof(long);
    }
    emulNet->ENsend(&memberNode->addr, &sendToMember, (char *)membershipListMsg, listMsgSize);
    free(membershipListMsg);
}



// send member list to defined number of random nodes
// don't sender a member on list that has failed (has not yet been removed)
void MP1Node::sendMembershipList()
{

       vector<MemberListEntry>::iterator liveCheckPosition;
     
     // go through this members list and add each entry to the message
    int numAlive = 0;
    for(liveCheckPosition = memberNode->memberList.begin();
        liveCheckPosition != memberNode->memberList.end();
        liveCheckPosition++)
    {   
        if(liveCheckPosition->gettimestamp()+TFAIL >= par->getcurrtime())      // if not true, then member has failed
        {
            numAlive++;
        }

    }
 //  cout << "number alive " << numAlive << " for member ID " << *(int *)(&memberNode->addr.addr) << endl;
  //  cout << "size of ML " << memberNode->memberList.size() << endl <<endl;
    // only one item on list. do not send
    if(memberNode->memberList.size() < 2)
    {
        return;
    }
    
    int msgPosition = 0;
    MessageHdr *membershipListMsg;
    size_t listMsgSize = sizeof sizeof(MessageHdr) + sizeof(Address) + sizeof(long) + sizeof(int) +
        (numAlive)*(sizeof(int)+sizeof(short)+sizeof(long));

    membershipListMsg = (MessageHdr *) malloc(listMsgSize);
    membershipListMsg->msgType = GOSSIP;
    msgPosition += sizeof(MsgTypes);    // move pointer to next position in the message
    memcpy((char *)(membershipListMsg)+msgPosition,&memberNode->addr.addr,sizeof(Address));
    msgPosition += sizeof(Address);
    memcpy((char *)(membershipListMsg) + msgPosition, &memberNode->heartbeat, sizeof(long));
    msgPosition += sizeof(long);
    memcpy((char *)(membershipListMsg) + msgPosition, &numAlive, sizeof(int));        // pass number of members in the list
    msgPosition += sizeof(int);

    vector<MemberListEntry>::iterator memberPosition;
     
     // go through this members list and add each entry to the message
    int tempPosition = 0;
    for(memberPosition = memberNode->memberList.begin();
        memberPosition != memberNode->memberList.end();
        memberPosition++)

    {  
        if(memberPosition->timestamp + TFAIL < par->getcurrtime())
        {
         //   cout << "possible fail at " << memberPosition->id << endl;
            continue;
        }
    //    cout << "memberPosition " << memberPosition->getid() << endl;
        memcpy((char *)(membershipListMsg)+msgPosition, &memberPosition->id, sizeof(int));
        msgPosition += sizeof(int);
        memcpy((char *)(membershipListMsg)+msgPosition, &memberPosition->port, sizeof(short));
        msgPosition += sizeof(short);
        memcpy((char *)(membershipListMsg)+msgPosition, &memberPosition->heartbeat, sizeof(long));
        msgPosition += sizeof(long);
    }

   // srand(time(NULL));
    for(int i = 0; i<NUMTOGOSSIP; i++)
    {   
        Address sendTo;

        //int sendToID = rand() % (numMembers-1) + 1; // randomly pick with member ID to sent to
        int sendToID = rand() % (memberNode->memberList.size()) + 1;
  //      cout << "Random node select is " << sendToID << " selected by " << nodeID << endl;
        memcpy(&sendTo.addr[0], &memberNode->memberList[sendToID-1].id, sizeof(int));
        memcpy(&sendTo.addr[4], &memberNode->memberList[sendToID-1].port, sizeof(short));
        emulNet->ENsend(&memberNode->addr, &sendTo, (char *)membershipListMsg, listMsgSize);
      
     
    }
    free(membershipListMsg);
}

// takes memberID and returns where in that member's membership list they are located
int MP1Node::getListPositionByAddress(Address memberAddr)
{
    int listLocation = 0;
    int thisID = (int)memberAddr.addr[0];
    short thisPort = (short)memberAddr.addr[4];
    int tempID;
    short tempPort;
 //   cout << "looking up ID " << thisID << " in ML " << endl;
    vector<MemberListEntry>::iterator memberPosition;
    for(memberPosition = memberNode->memberList.begin();
        memberPosition != memberNode->memberList.end();
        memberPosition++)
    {
        tempID = memberPosition->id;
        tempPort = memberPosition->port;
//        cout << "look up member. this ID: " << thisID << " list position id: " << tempID << " list location: " << listLocation << endl;
        if((tempID == thisID) && (tempPort == thisPort))
        {
            return listLocation;
        }
        listLocation++;
    }
    return(-1); // no member found;
}

/*
// ********************************************************************** not used ******************************
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


// issues
// keeps remove failed node...so not actually getting removed
// does not appear get re-added, based on log output



