/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

// Homework 4
// Colin Banigan and Katherine Drake
// CSCE 438 Section 500
// April 14, 2017

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <stdlib.h>
#include <thread>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "fb.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::ClientContext;
using grpc::Status;
using grpc::Channel;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using hw4::Message;
using hw4::WorkerAddress;
using hw4::Request;
using hw4::Reply;
using hw4::AssignedWorkers;
using hw4::MessengerMaster;
using hw4::MessengerWorker;
using hw4::ClientListReply;
using hw4::CreateWorkerRequest;
using hw4::ListReply;
using hw4::ClientData;
using hw4::WorkerData;

using namespace std;

//Worker struct
class WorkerProcess {
    public:
    string hostname;
    string portnumber;
    unique_ptr<MessengerWorker::Stub> workerStub;
    int vectorClockIndex;
    
    WorkerProcess(){
        hostname = "";
        portnumber= "";
        vectorClockIndex = 0;
        workerStub = NULL;
    }
    
    WorkerProcess(string h, string p) {
        hostname = h;
        portnumber = p;
        vectorClockIndex = 0;
        
        shared_ptr<Channel> channel = grpc::CreateChannel(hostname + ":" + portnumber, grpc::InsecureChannelCredentials());
        workerStub = MessengerWorker::NewStub(channel);
    }
    
    WorkerProcess(string h, string p, shared_ptr<Channel> c, int clock){
        hostname = h;
        portnumber = p;
        workerStub = MessengerWorker::NewStub(c);
        vectorClockIndex = clock;
    }
    
    string getWorkerAddress() {
        return hostname + ":" + portnumber;
    }
};

//Client struct that holds a user's username, followers, and users they follow
struct Client {
    string username;
   
    // usernames for the clients the user follows
    vector<string> clientFollowers;
    
    // usernames for the clients who follow the user
    vector<string> clientFollowing;
    
    // pointer to workers assigned to client
    string primaryWorker;
    string secondary1Worker;
    string secondary2Worker;
    
    Client() {
        username = "";
        clientFollowers = vector<string>();
        clientFollowing = vector<string>();
        primaryWorker = "";
        secondary1Worker = "";
        secondary2Worker = "";
    }
    
    Client(string uname) {
        username = uname;
        clientFollowers = vector<string>();
        clientFollowing = vector<string>();
        primaryWorker = "";
        secondary1Worker = "";
        secondary2Worker = "";
    }
    
    bool operator==(const Client& c1) const{
        return (username == c1.username);
    }
};

class MasterToMasterConnection;

// notes if process is a master or master replica
// if true, this is the master process
// if false, this is a master replica
bool isMaster = true;

vector<MasterToMasterConnection*> masterConnection = vector<MasterToMasterConnection*>();

//Vector that stores every client that has been created
vector<Client*> clientDB;

int findUser(string username){
    int index = 0;
    for(Client* c : clientDB){
        if(c->username == username){
            return index;
        }
        index++;
    }
    return -1;
}

//Helper function used to find a Client object given its username
bool checkIfUserExists(string username){
    for(Client* c : clientDB){
        if(c->username == username){
            return true;
        }
    }
    return false;
}

string masterAddress = "";
vector<WorkerProcess*> listWorkers = vector<WorkerProcess*>();

WorkerProcess* getWorker(string address) {
    for(WorkerProcess* w : listWorkers) {
        if(w->getWorkerAddress() == address) {
            return w;
        }
    }
    return NULL;
}

int findWorker(string address){
    int index = 0;
    for(WorkerProcess* w : listWorkers){
        if(w->getWorkerAddress() == address){
            return index;
        }
        index++;
    }
    return -1;
}

//hostname and portnumber for the master
string master_hostname;
string master_portnumber;

class MasterToMasterConnection {
    public:
    string connectedMasterAddress;
    unique_ptr<MessengerMaster::Stub> masterStub;
    
    MasterToMasterConnection(string maddress, shared_ptr<Channel> channel) {
        connectedMasterAddress = maddress;
        masterStub = MessengerMaster::NewStub(channel);
    }
    
    // establishes a connection between a master and master replica
    void ReplicaConnectToMaster() {
        //keep on re-running until we connect to the master
        bool connected = false;
        while(!connected){
            // Data being sent to the master
            Request request;
            request.set_username("Master Replica");
            request.add_arguments(masterAddress);

            // Container for the data from the master
            Reply reply;

            // Context for the client
            ClientContext context;
            
            Status status = masterStub->MasterReplicaConnected(&context, request, &reply);
            if(status.ok()){
                connected = true;
                return;
            }
        }
    }
    
    // Sends new/updated database entry for a client to a master replica
    void UpdateReplicaClient(Client* client){
        
        ClientData request;
        request.set_username(client->username);
        
        for(int i = 0; i < client->clientFollowers.size(); i++){
            request.add_followers(client->clientFollowers[i]);
        }
        
        for(int i = 0; i < client->clientFollowing.size(); i++){
            request.add_following(client->clientFollowing[i]);
        }
        
        request.set_primary(client->primaryWorker);
        request.set_secondary1(client->secondary1Worker);
        request.set_secondary2(client->secondary2Worker);
        
        Reply reply;
        
        ClientContext context;
        
        Status status = masterStub->UpdateReplicaClient(&context, request, &reply);
        if(!status.ok()){
            cout << "ERROR: failed to update client data on master replica" << endl;
        }
    }
    
    // Sends new/updated database entry for a worker to a master replica
    void UpdateReplicaWorker(string hostname, string portnumber, int clock, string operation){
        
        WorkerData request;
        request.add_host(hostname);
        request.add_port(portnumber);
        request.add_clock(clock);
        request.set_operation(operation);
        
        Reply reply;
        
        ClientContext context;
        
        Status status = masterStub->UpdateReplicaWorker(&context, request, &reply);
        if(!status.ok()){
            cout << "ERROR: failed to update worker data on master replica" << endl;
        }
    }
};

// Logic and data behind the server's behavior.
class MessengerServiceMaster final : public MessengerMaster::Service {
  
   Status WorkerConnected(ServerContext* context, const WorkerAddress* request, Reply* reply) override {
       string hostname = request->host();
       string portnumber = request->port();
       int clockIndex = request->clock();
       
       shared_ptr<Channel> workerChannel = grpc::CreateChannel(hostname + ":" + portnumber, grpc::InsecureChannelCredentials());
       WorkerProcess* worker = new WorkerProcess(hostname, portnumber, workerChannel, clockIndex);
       
       cout << "Master - New Worker Connected: " << worker->getWorkerAddress() << endl;
       
       //check to see if worker already exists
       bool alreadyExists = false;
       for(int i = 0; i < listWorkers.size(); i++) {
           if(listWorkers[i]->getWorkerAddress() == (hostname + ":" + portnumber)){
               listWorkers[i] = worker;
               alreadyExists = true;
           }
       }
       
       if(!alreadyExists) {
           listWorkers.push_back(worker);
           
           //update replicas that a new worker was added
           for(int i = 1; i < masterConnection.size(); i++){
               masterConnection[i]->UpdateReplicaWorker(hostname, portnumber, clockIndex, "Add");
           }
       }

       return Status::OK;
   }

    Status FindPrimaryWorker(ServerContext* context, const Request* request, AssignedWorkers* reply) override {
        
        string clientUsername = request->username();
       
        int indexPrimary = -1;
        int indexSecondary1 = -1;
        int indexSecondary2 = -1;
        
        int currentMin = 999999;
        
        //initial loop to find the index for the primary worker
        for(int i = 0; i < listWorkers.size(); i++){
            
            ClientContext clientContext;
            Request clientRequest;
            Reply clientReply;
            
            Status status = listWorkers[i]->workerStub->NumberClientsConnected(&clientContext, clientRequest, &clientReply);
            
            if(status.ok()) {
                if(stoi(clientReply.msg()) < currentMin){
                    indexPrimary = i;
                    currentMin = stoi(clientReply.msg());
                }
            }
            else {
                cout << status.error_code() << ": " << status.error_message()
                    << endl;
                cout << "Master - RPC failed\n";
            }
        }
        
        currentMin = 999999;
        
        //loop to find the index for the first secondary worker
        for(int i = 0; i < listWorkers.size(); i++){
            
            ClientContext clientContext;
            Request clientRequest;
            Reply clientReply;
            
            Status status = listWorkers[i]->workerStub->NumberClientsConnected(&clientContext, clientRequest, &clientReply);
      
            if(status.ok()) {
                if(stoi(clientReply.msg()) < currentMin && listWorkers[i]->hostname != listWorkers[indexPrimary]->hostname){
                    indexSecondary1 = i;
                    currentMin = stoi(clientReply.msg());
                }
            }
            else {
                cout << status.error_code() << ": " << status.error_message()
                    << endl;
                cout << "RPC failed\n";
            }
        }
        
        currentMin = 999999;
        
        //loop to find the index for the second secondary worker
        for(int i = 0; i < listWorkers.size(); i++){
            
            ClientContext clientContext;
            Request clientRequest;
            Reply clientReply;
            
            Status status = listWorkers[i]->workerStub->NumberClientsConnected(&clientContext, clientRequest, &clientReply);
      
            if(status.ok()) {
                if(stoi(clientReply.msg()) < currentMin && (listWorkers[i]->hostname != listWorkers[indexPrimary]->hostname && listWorkers[i]->hostname != listWorkers[indexSecondary1]->hostname)){
                    indexSecondary2 = i;
                    currentMin = stoi(clientReply.msg());
                }
            }
            else {
                cout << status.error_code() << ": " << status.error_message()
                    << endl;
                cout << "RPC failed\n";
            }
        }
        
        // if user is not in the database yet, userIndex=-1, meaning that they have connected for the first time
        // if userIndex=-1, don't set their primary and secondary workers, THEY ARENT IN THE DB YET!!!!
        int userIndex = findUser(clientUsername);
        
        // set primary worker for client
        if(indexPrimary != -1){
            string primaryAddress = listWorkers[indexPrimary]->getWorkerAddress();
            reply->set_primary(primaryAddress);
            if(userIndex != -1) {
                clientDB[userIndex]->primaryWorker = primaryAddress;
            }
        } else {
            reply->set_primary("NONE");
            if(userIndex != -1) {
                clientDB[userIndex]->primaryWorker = "NONE";
            }
            
        }
        
        // set secondary1 worker for client
        if(indexSecondary1 != -1){
            string secondary1Address = listWorkers[indexSecondary1]->getWorkerAddress();
            reply->set_secondary1(secondary1Address);
            if(userIndex != -1) {
                clientDB[userIndex]->secondary1Worker = secondary1Address;
            }
        } else {
            reply->set_secondary1("NONE");
            if(userIndex != -1) {
                clientDB[userIndex]->secondary1Worker = "NONE";
            }
        }
        
        // set secondary2 worker for client
        if(indexSecondary2 != -1){
            string secondary2Address = listWorkers[indexSecondary2]->getWorkerAddress();
            reply->set_secondary2(secondary2Address);
            if(userIndex != -1) {
                clientDB[userIndex]->secondary2Worker = secondary2Address;
            }
        } else {
            reply->set_secondary2("NONE");
            if(userIndex != -1) {
                clientDB[userIndex]->secondary2Worker = "NONE";
            }
        }
        
        return Status::OK;
    }
    
    // Get the address for the specified client's primary worker
    Status GetClientsPrimaryWorker(ServerContext* context, const Request* request, Reply* reply) override {
        //cout << "Master - In GetClientsPrimaryWorker\n";
        string username = request->username();
        int clientIndex = findUser(username);
        
        // check if client was found in the database
        if (clientIndex == -1) {
            reply->set_msg("Username not found");
            return Status::CANCELLED;
        }
        
        string workerAddress = clientDB[clientIndex]->primaryWorker;
        
        reply->set_msg(workerAddress);
        
        return Status::OK;
    }
    
    Status LoginMaster(ServerContext* context, const Request* request, Reply* reply) override {
        string username = request->username();
        string primaryAddress = request->arguments(0);
        string secondary1Address = request->arguments(1);
        string secondary2Address = request->arguments(2);
        
        //if the username does not already exist, add it to the database
        if(!checkIfUserExists(username)){
            Client* client = new Client(username);
            
            client->primaryWorker = primaryAddress;
            client->secondary1Worker = secondary1Address;
            client->secondary2Worker = secondary2Address;
            
            clientDB.push_back(client);
            reply->set_msg("Login Successful!");
            
            //update replicas that a new client was added
            for(int i = 1; i < masterConnection.size(); i++){
               masterConnection[i]->UpdateReplicaClient(client);
            }
            
            
        } else {
            reply->set_msg("Welcome Back " + username);
        }
        
        return Status::OK;
    }
    
    Status JoinMaster(ServerContext* context, const Request* request, Reply* reply) override {
        string username = request->username();
        string usernameToJoin = request->arguments(0);
        
        //if both of the usernames exist, join username with usernameToJoin
        if(checkIfUserExists(username) && checkIfUserExists(usernameToJoin)){
            
            int userIndex = findUser(username);
            
            //check to see if join has already happened
            bool exists = false;
            for(int i = 0; i < clientDB[userIndex]->clientFollowing.size(); i++){
                if(clientDB[userIndex]->clientFollowing[i] == usernameToJoin){
                    exists = true;
                }
            }
            
            //if it hasn't, join now
            if(!exists){
               clientDB[userIndex]->clientFollowing.push_back(usernameToJoin); 
            }
            
            int userJoinIndex = findUser(usernameToJoin);
            
            //check to see if join has already happened
            exists = false;
            for(int i = 0; i < clientDB[userJoinIndex]->clientFollowers.size(); i++){
                if(clientDB[userJoinIndex]->clientFollowers[i] == username){
                    exists = true;
                }
            }
            
            //if it hasn't, join now
            if(!exists){
                clientDB[userJoinIndex]->clientFollowers.push_back(username);
            }
            
            reply->set_msg("Join Successful!");
        } else {
            reply->set_msg("ERROR: Join Unsuccessful.");
        }
        
        return Status::OK;
    }
    
    Status LeaveMaster(ServerContext* context, const Request* request, Reply* reply) override {
        string username = request->username();
        string usernameToLeave = request->arguments(0);
        
        //if both of the usernames exist, join username with usernameToJoin
        if(checkIfUserExists(username) && checkIfUserExists(usernameToLeave)){
            
            //check to see if leave has already happened. If it hasn't, leave now
            int userIndex = findUser(username);
            
            bool exists = false;
            for(int i = 0; i < clientDB[userIndex]->clientFollowing.size(); i++){
                if(clientDB[userIndex]->clientFollowing[i] == usernameToLeave){
                    exists = true;
                }
            }
            if(exists){
                clientDB[userIndex]->clientFollowing.erase(find(clientDB[userIndex]->clientFollowing.begin(), clientDB[userIndex]->clientFollowing.end(), usernameToLeave));
            }
            
            //check to see if leave has already happened. If it hasn't, leave now
            int userLeaveIndex = findUser(usernameToLeave);
            
            exists = false;
            for(int i = 0; i < clientDB[userLeaveIndex]->clientFollowers.size(); i++){
                if(clientDB[userLeaveIndex]->clientFollowers[i] == username){
                    exists = true;
                }
            }
            if(exists){
                clientDB[userLeaveIndex]->clientFollowers.erase(find(clientDB[userLeaveIndex]->clientFollowers.begin(), clientDB[userLeaveIndex]->clientFollowers.end(), username));
            }
            
            reply->set_msg("Leave Successful!");
        } else {
            reply->set_msg("ERROR: Leave Unsuccessful.");
        }
        
        return Status::OK;
    }
    
    Status ListMaster(ServerContext* context, const Request* request, Reply* reply) override {
        
        // empty string to return with the list data
        string totalList = "";
        
        totalList += "-------------------------------------------------------------\n";

        // iterate through all of the users
        for (int i = 0; i < clientDB.size(); i++) {

            // our name
            totalList += "User: " + clientDB[i]->username + "\n";
            
            // people who we follow
            totalList += "Following: [";
            for (int j = 0; j < clientDB[i]->clientFollowing.size(); j++) {
                totalList += clientDB[i]->clientFollowing[j];
                if (j != clientDB[i]->clientFollowing.size() - 1) {
                    totalList += ", ";
                }
            }
            totalList += "]\n";
            
            // people who follow us
            totalList += "Followers: [";
            for (int j = 0; j < clientDB[i]->clientFollowers.size(); j++) {
                totalList += clientDB[i]->clientFollowers[j];
                if (j != clientDB[i]->clientFollowers.size() - 1) {
                    totalList += ", ";
                }
            }
            totalList += "]\n";
            
            totalList += "-------------------------------------------------------------\n";
        }
        
        reply->set_msg(totalList);
        return Status::OK;
    }
    
    Status UpdateClientData(ServerContext* context, const Request* request, ClientListReply* reply) override {
        string username = request->username();
        
        //set username
        reply->set_username(username);
        
        int userIndex = findUser(username);
        
        if(userIndex != -1){
            //set followers
            for(int i = 0; i < clientDB[userIndex]->clientFollowers.size(); i++){
                reply->add_followers(clientDB[userIndex]->clientFollowers[i]);
            }

            //set following
            for(int i = 0; i < clientDB[userIndex]->clientFollowing.size(); i++){
                reply->add_following(clientDB[userIndex]->clientFollowing[i]);
            }
        } else {
            cout << "ERROR: user not found in the database in UpdateClientData";
        }
        return Status::OK;
    }
    
    Status AskForFile(ServerContext* context, const Request* request, ListReply* reply) override {
        
        string file = request->arguments(0) + ".txt";
        
        string line;
        ifstream i(file);
        if(!i.fail()){
            while(getline(i,line)) {
                reply->add_msgs(line);
            }
        }
        
        return Status::OK;
    }
    
    // returns a list of all client usernames currently in the database
    Status GetAllClients(ServerContext* context, const Request* request, ListReply* reply) override {
        
        for(int i = 0; i < clientDB.size(); i++){
            reply->add_msgs(clientDB[i]->username);
        }
        
        return Status::OK;
    }
    
    // returns addresses for all workers currently in the database
    // called when a new master replica process is started
    Status GetAllWorkers(ServerContext* context, const Request* request, WorkerData* reply) override {
        
        for(WorkerProcess* w : listWorkers) {
            reply->add_host(w->hostname);
            reply->add_port(w->portnumber);
        }
        
        reply->set_operation("Add");
        
        return Status::OK;
    }
    
    // sends updated data for a client
    // master sends data to the replica each time a client's data is changed
    Status UpdateReplicaClient(ServerContext* context, const ClientData* request, Reply* reply) override {
        Client* client;
        
        vector<string> followers = vector<string>();
        vector<string> following = vector<string>();
        
        // retrieve all followers from request
        for(string s : request->followers()) {
            followers.push_back(s);
        }
        
        // retrieve all following from request
        for(string s : request->following()) {
            following.push_back(s);
        }
        
        // lookup client's username in the database
        int clientIndex = findUser(request->username());
        
        if(clientIndex != -1) {
            // if client is already in the database, reset data
            client = clientDB[clientIndex];
            
            client->clientFollowers = followers;
            client->clientFollowing = following;
            client->primaryWorker = request->primary();
            client->secondary1Worker = request->secondary1();
            client->secondary2Worker = request->secondary2();
        }
        else {
            // if client isn't in the database, add it 
            client->username = request->username();
            client->clientFollowers = followers;
            client->clientFollowing = following;
            client->primaryWorker = request->primary();
            client->secondary1Worker = request->secondary1();
            client->secondary2Worker = request->secondary2();
            
            clientDB.push_back(client);
        }
        reply->set_msg("Success");
        return Status::OK;
    }
    
    // sends updated data for a worker
    // master sends data to the replica each time a worker is added/deleted
    Status UpdateReplicaWorker(ServerContext* context, const WorkerData* request, Reply* reply) override {
        string hostname = request->host(0);
        string portnumber = request->port(0);
        int clock = request->clock(0);
        string workerAddress = hostname + ":" + portnumber;
        
        // add a new worker to the database
        if(request->operation() == "Add") {
            // check if a worker at the address already exists
            if(getWorker(workerAddress) != NULL) {
                return Status::CANCELLED;
            }
            
            // create a channel to the new worker
            shared_ptr<Channel> workerChannel = grpc::CreateChannel(hostname + ":" + portnumber, grpc::InsecureChannelCredentials());
            
            WorkerProcess* worker = new WorkerProcess(hostname, portnumber, workerChannel, clock);
            
            // add the new worker to the database
            listWorkers.push_back(worker);
        }
        // delete an existing worker from the database
        else if(request->operation() == "Delete") {
            // get index for worker in the database
            int workerIndex = findWorker(workerAddress);
            
            if(workerIndex == -1) {
                return Status::CANCELLED;
            }
            listWorkers.erase(listWorkers.begin()+workerIndex);
        }
        else {
            return Status::CANCELLED;
        }
        
        reply->set_msg("Success");
        return Status::OK;
    }
    
    // master replica sends its address to the master process
    Status MasterReplicaConnected(ServerContext* context, const Request* request, Reply* reply) override {
        string replicaAddress = request->arguments(0);
        shared_ptr<Channel> channel = grpc::CreateChannel(replicaAddress, grpc::InsecureChannelCredentials());
    
        // add connection to the new master replica to the database
        MasterToMasterConnection* replica = new MasterToMasterConnection(replicaAddress, channel);
        masterConnection.push_back(replica);
        
        cout << "Master - Connected to a Replica: " << replicaAddress << endl;
        
        reply->set_msg("Success");
        return Status::OK;
    }
    
    Status GetWorkerOnHost(ServerContext* context, const Request* request, Reply* reply) override {
        string host = request->arguments(0);
        
        for(WorkerProcess* w : listWorkers) {
            if(w->hostname == host) {
                // heartbeat worker to make sure it's still active
                Request requestHB;
                Reply replyHB;
                ClientContext clientContext;
                
                Status status = w->workerStub->CheckWorker(&clientContext, requestHB, &replyHB);
                
                if(status.ok()){
                    reply->set_msg(w->getWorkerAddress());
                    return Status::OK;
                }
            }
        }
        reply->set_msg("Failure");
        return Status::OK;
    }
    
    Status MasterHeartbeat(ServerContext* context, const Request* request, Reply* reply) override {
        reply->set_msg("lub-DUB");
        return Status::OK;
    }
};

// Used to connect master and master replica processes together
void ConnectToMaster(string port) {
    string address = master_hostname + ":" + port;
    shared_ptr<Channel> channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    
    MasterToMasterConnection* master = new MasterToMasterConnection(address, channel);
    masterConnection.push_back(master);
}

void* RunMaster(void* v) {
    string address = master_hostname + ":" + master_portnumber;
    MessengerServiceMaster service;

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    
    // Finally assemble the server.
    unique_ptr<Server> master(builder.BuildAndStart());
    if(isMaster) {
        cout << "Master - Master listening on " << address << endl;
    }
    else {
        cout << "Master Replica - Replica listening on " << address << endl;
    }

    //setting up MasterAddress
    masterAddress = address;

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    
    master->Wait();
    //thread masterThread(threadWait, master);
}

//heartbeat function
void* Heartbeat(void* v){
    while(true){
        for(int i = 0; i < listWorkers.size(); i++){
            //MAY WANT TO ADD A SLEEP IN HERE JUST SO WE DON'T FLOOD OURSELVES LMAO
            
            //Send lub-DUB
            Request request;
            Reply reply;
            ClientContext context;
            Status status = listWorkers[i]->workerStub->CheckWorker(&context, request, &reply);
            
            if(!status.ok()){
                cout << "lub-DUB FAILED, ABORT ABORT!! DO SOMETHING HERE" << endl;
                
                /*
                * if lub-dub fails
                * see if other workers on the same server are alive
                * if so, one of those workers needs to restart another worker
                * write a helper function to let everybody know that some dudes lub-dubber be no longer lub-dubbin'
                * if no-one else responds from the helper function, we know that the entire server is dead
                */
                
                int deadIndex = i;
                string serverToCheck = listWorkers[deadIndex]->hostname;
                bool isServerDead = false;
                
                //look for a worker on the same server, see if it's only our worker or the whole server is dead
                for(int j = 0; j < listWorkers.size(); j++){
                    if(j != deadIndex && serverToCheck == listWorkers[j]->hostname && serverToCheck != master_hostname){
                        
                        //Send lub-DUB
                        Request requestInner;
                        Reply replyInner;
                        ClientContext contextInner;
                        Status statusInner = listWorkers[j]->workerStub->CheckWorker(&contextInner, requestInner, &replyInner);
                        
                        if(!statusInner.ok()){
                            isServerDead = true;
                        }
                    }
                }
                
                if(isServerDead){
                    //server's dead, do what we need to do in here
                    
                    //remove dead worker from the database
                    listWorkers.erase(listWorkers.begin()+deadIndex);
                    
                    //new workers will be started via the script once the server is back on
                    
                } else {
                    
                    //worker's dead, do what we need to do in here
                    //Need to do: re-run worker on address found in i
                    
                    string workerHostname = listWorkers[deadIndex]->hostname;
                    string deadPortnumber = listWorkers[deadIndex]->portnumber;
                    int clockIndex = listWorkers[deadIndex]->vectorClockIndex;
                    
                    //loop through the current workers on our hostname, find the largest port and increment it by 1 for our new port
                    int nPort = stoi(master_portnumber);
                    for(int j = 0; j < listWorkers.size(); j++){
                        if(listWorkers[j]->hostname == workerHostname && nPort < stoi(listWorkers[j]->portnumber)){
                            nPort = stoi(listWorkers[j]->portnumber);
                        }
                    }
                    //check against Master Replica as well
                    if(stoi(masterConnection[0]->connectedMasterAddress.substr(25, 4)) > nPort){
                        nPort = stoi(masterConnection[0]->connectedMasterAddress.substr(25, 4));
                    }
                    string workerPort = to_string(nPort+1);
                    
                    //remove dead worker from the database
                    listWorkers.erase(listWorkers.begin()+deadIndex);
                    
                    //update replicas that a worker was removed
                    for(int i = 1; i < masterConnection.size(); i++){
                       masterConnection[i]->UpdateReplicaWorker(workerHostname, deadPortnumber, clockIndex, "Delete");
                    }
                    
                    //if the dead worker is on the master's server, we can have the master start it
                    if(workerHostname == master_hostname){

                        pid_t child = fork();
                        if(child == 0){
                            char* argv[11];

                            //string systemArgs = "./worker -h " + workerHostname + " -p " + workerPort + " -m " + masterHostname + " -a " + masterPort + " &";
                            vector<string> args = {"./worker", "-h", workerHostname, "-p", workerPort, "-m", master_hostname, "-a", master_portnumber, "&"};

                            for(int j = 0; j < args.size(); j++){
                                string a = args[j];
                                argv[j] = (char *)a.c_str();
                            }

                            argv[10] = NULL;

                            execv("./worker", argv);
                        }
                        
                        cout << "created new worker process on " << workerHostname << ":" << workerPort << endl;
                        
                    } else {
                        
                        CreateWorkerRequest requestWorker;
                        requestWorker.set_worker_hostname(workerHostname);
                        requestWorker.set_worker_port(workerPort);

                        requestWorker.set_master_hostname(master_hostname);
                        requestWorker.set_master_port(master_portnumber);

                        Reply replyWorker;
                        ClientContext contextWorker;
                        
                        //find a worker on the hostname we want, save that index
                        int createWorkerIndex = -1;
                        for(int j = 0; j < listWorkers.size(); j++){
                            if(listWorkers[j]->hostname == workerHostname){
                                createWorkerIndex = j;
                            }
                        }
                        
                        //check to see if we found the hostname
                        if(createWorkerIndex != -1){
                            Status statusWorker = listWorkers[createWorkerIndex]->workerStub->StartNewWorker(&contextWorker, requestWorker, &replyWorker);

                            if(statusWorker.ok()){
                                cout << "created new worker process on " << workerHostname << ":" << workerPort << endl;
                            } else {
                                cout << "ERROR: couldn't create new worker process on " << workerHostname << ":" << workerPort << endl;
                            } 
                        } else {
                            cout << "ERROR: could not find worker on " << workerHostname << " to create a new worker process on" << endl;
                        }
                    }
                    
                }
            } else {
                //FOR TESTING PURPOSES ONLY
                //cout << "lub-Dub" << endl;
            }
        }
    }
}

//master replica heartbeat to the master
void* MasterHeartbeat(void* v){
    while(!isMaster){
            
        //Send master lub-DUB
        Request request;
        Reply reply;
        ClientContext context;
        Status status = masterConnection[0]->masterStub->MasterHeartbeat(&context, request, &reply);

        if(!status.ok()){
            cout << "Master lub-DUB FAILED" << endl;
            
            //calculate new port number location
            int nPort = stoi(master_portnumber);
            for(int j = 0; j < listWorkers.size(); j++){
                if(listWorkers[j]->hostname == master_hostname && nPort < stoi(listWorkers[j]->portnumber)){
                    nPort = stoi(listWorkers[j]->portnumber);
                }
            }
            string masterReplicaPortnumber = to_string(nPort+1);
            
            //run a new master process as a replica
            pid_t child = fork();
            if(child == 0){
                char* argv[11];

                // ./master -h lenss-comp1.cse.tamu.edu -p 4233 -r replica -m 4232 &
                vector<string> args = {"./master", "-h", master_hostname, "-p", masterReplicaPortnumber, "-r", "replica", "-m", master_portnumber, "&"};

                for(int i = 0; i < args.size(); i++){
                    string a = args[i];
                    argv[i] = (char *)a.c_str();
                }

                argv[10] = NULL;

                execv("./master", argv);
            }
            cout << "created new master replica process on " << master_hostname << ":" << masterReplicaPortnumber << endl;
            
            //inform all of the workers that we are the new master
            for(int i = 0; i < listWorkers.size(); i++){
                Request request;
                request.add_arguments(master_hostname);
                request.add_arguments(masterReplicaPortnumber);
                Reply reply;
                ClientContext context;
                Status status = listWorkers[i]->workerStub->UpdateMaster(&context, request, &reply);
                
                if(!status.ok()){
                    cout << "ERROR: worker did not update master address" << endl;
                }
            }
            
            //change this process to the master
            masterConnection.erase(masterConnection.begin());
            isMaster = true;
        }
    }
}

int main(int argc, char** argv) {
    
    //default
    master_portnumber = "4632";
    
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:p:r:m:")) != -1){
        switch(opt) {
            case 'h':
                master_hostname = optarg;
                break;
            case 'p':
                master_portnumber = optarg;
                break;
            case 'r':
                isMaster = false;
                break;
            case 'm':
                ConnectToMaster(optarg);
                break;
            default:
                cerr << "Invalid Command Line Argument\n";
        }
    }
    masterAddress = master_hostname + ":" + master_portnumber;
    
    pthread_t masterThread;
	pthread_create(&masterThread, NULL, RunMaster, NULL);
    
    // master starts to heartbeat the workers
    if(!isMaster) {
        masterConnection[0]->ReplicaConnectToMaster();
        
        pthread_t masterHeartbeatThread;
        pthread_create(&masterHeartbeatThread, NULL, MasterHeartbeat, NULL);
    }
    
    while(!isMaster) {
        continue;
    }
    
    pthread_t heartbeatThread;
    pthread_create(&heartbeatThread, NULL, Heartbeat, NULL);
    
    while(true) {
        continue;
    }
    
    cout << "Master - Master is Shutting Down\n";
    
    return 0;
}
