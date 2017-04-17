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

using namespace std;

//Worker struct
class WorkerProcess {
    public:
    string hostname;
    string portnumber;
    unique_ptr<MessengerWorker::Stub> workerStub;
    
    WorkerProcess(){
        hostname = "";
        portnumber= "";
        workerStub = NULL;
    }
    
    WorkerProcess(string h, string p) {
        hostname = h;
        portnumber = p;
        
        shared_ptr<Channel> channel = grpc::CreateChannel(hostname + ":" + portnumber, grpc::InsecureChannelCredentials());
        workerStub = MessengerWorker::NewStub(channel);
    }
    
    WorkerProcess(string h, string p, shared_ptr<Channel> c){
        hostname = h;
        portnumber = p;
        workerStub = MessengerWorker::NewStub(c);
    }
    
    string getWorkerAddress() {
        return hostname + ":" + portnumber;
    }
};

//Client struct that holds a user's username, followers, and users they follow
struct Client {
    string username;
   
    //int following_file_size = 0;
    
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

//hostname and portnumber for the master
string master_hostname;
string master_portnumber;

// Logic and data behind the server's behavior.
class MessengerServiceMaster final : public MessengerMaster::Service {
  
   Status WorkerConnected(ServerContext* context, const WorkerAddress* request, Reply* reply) override {
       cout << "Master - New Worker Connected to Master\n";
       
       string hostname = request->host();
       string portnumber = request->port();
       
       shared_ptr<Channel> workerChannel = grpc::CreateChannel(hostname + ":" + portnumber, grpc::InsecureChannelCredentials());
       WorkerProcess* worker = new WorkerProcess(hostname, portnumber, workerChannel);
       
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
       }
       
       cout << "Master, list workers size in WorkerConnected: " << listWorkers.size() << "\n";
       
       return Status::OK;
   }

    Status FindPrimaryWorker(ServerContext* context, const Request* request, AssignedWorkers* reply) override {
        
        string clientUsername = request->username();
       
        int indexPrimary = -1;
        int indexSecondary1 = -1;
        int indexSecondary2 = -1;
        
        int currentMin = 999999;
        
        cout << "Master, FindPrimayWorker, listWorkers size: " << listWorkers.size() << endl;
        
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
                cout << clientReply.msg() << endl;
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
        
        
        /* 
            TO DO
            
            Use clientUsername to find the client in the client DB
            Add primary, secondary1, and secondary2 workers to the client object
        
        */
        
        // if user is not in the database yet, userIndex=-1, meaning that they have connected for the first time
        // if userIndex=-1, don't set their primary and secondary workers, THEY ARENT IN THE DB YET!!!!
        int userIndex = findUser(clientUsername);
        
        if(indexPrimary != -1){
            string primaryAddress = listWorkers[indexPrimary]->getWorkerAddress();
            reply->set_primary(primaryAddress);
            if(userIndex != -1) {
                clientDB[userIndex]->primaryWorker = primaryAddress;
            }
        } else {
            reply->set_primary("NONE");
            if(userIndex != -1) {
                clientDB[userIndex]->primaryWorker = "";
            }
            
        }
        
        if(indexSecondary1 != -1){
            string secondary1Address = listWorkers[indexSecondary1]->getWorkerAddress();
            reply->set_secondary1(secondary1Address);
            if(userIndex != -1) {
                clientDB[userIndex]->secondary1Worker = secondary1Address;
            }
        } else {
            reply->set_secondary1("NONE");
            if(userIndex != -1) {
                clientDB[userIndex]->secondary1Worker = "";
            }
        }
        
        if(indexSecondary2 != -1){
            string secondary2Address = listWorkers[indexSecondary2]->getWorkerAddress();
            reply->set_secondary2(secondary2Address);
            if(userIndex != -1) {
                clientDB[userIndex]->secondary2Worker = secondary2Address;
            }
        } else {
            reply->set_secondary2("NONE");
            if(userIndex != -1) {
                clientDB[userIndex]->secondary2Worker = "";
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
            for(int i = 0; i < clientDB[userIndex]->clientFollowers.size(); i++){
                if(clientDB[userIndex]->clientFollowers[i] == usernameToJoin){
                    exists = true;
                }
            }
            
            //if it hasn't, join now
            if(!exists){
               clientDB[userIndex]->clientFollowers.push_back(usernameToJoin); 
            }
            
            int userJoinIndex = findUser(usernameToJoin);
            
            //check to see if join has already happened
            exists = false;
            for(int i = 0; i < clientDB[userJoinIndex]->clientFollowing.size(); i++){
                if(clientDB[userJoinIndex]->clientFollowing[i] == username){
                    exists = true;
                }
            }
            
            //if it hasn't, join now
            if(!exists){
                clientDB[userJoinIndex]->clientFollowing.push_back(username);
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
            clientDB[userIndex]->clientFollowers.erase(find(clientDB[userIndex]->clientFollowers.begin(), clientDB[userIndex]->clientFollowers.end(), usernameToLeave)); 
            
            //check to see if leave has already happened. If it hasn't, leave now
            int userLeaveIndex = findUser(usernameToLeave);
            clientDB[userLeaveIndex]->clientFollowing.erase(find(clientDB[userLeaveIndex]->clientFollowing.begin(), clientDB[userLeaveIndex]->clientFollowing.end(), username)); 
            
            reply->set_msg("Leave Successful!");
        } else {
            reply->set_msg("ERROR: Leave Unsuccessful.");
        }
        
        return Status::OK;
    }
    
    Status ListMaster(ServerContext* context, const Request* request, Reply* reply) override {
        
        // empty string to return with the list data
        string totalList = "";

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
};

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
    cout << "Master - Master listening on " << address << endl;

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
                } else {
                    //worker's dead, do wwhta we need to do in here
                }
            } else {
                //FOR TESTING PURPOSES ONLY
                //cout << "lub-Dub" << endl;
            }
        }
    }
}

int main(int argc, char** argv) {
    
    //default
    master_portnumber = "4632";
    
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:p:")) != -1){
        switch(opt) {
            case 'h':
                master_hostname = optarg;
                break;
            case 'p':
                master_portnumber = optarg;
                break;
            default:
                cerr << "Invalid Command Line Argument\n";
        }
    }
    
    pthread_t masterThread;
	pthread_create(&masterThread, NULL, RunMaster, NULL);
    
    cout << "Master - Thread started\n";
    
    //pthread_t heartbeatThread;
	//pthread_create(&heartbeatThread, NULL, Heartbeat, NULL);
    
    cout << "Master - Heartbeat Thread started\n";
    
    while(true) {
        continue;
    }
    
    cout << "Master - Master is Shutting Down\n";
    
    return 0;
}
